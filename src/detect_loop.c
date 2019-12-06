#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <complex.h>
#include "fft.h"
#include "detect_loop.h"
#define THREAD_NUM 16

/*Calculate the mask*/
void calc_mask(int *mask, DDS_FreqsRecord_Typedef *DDS_PTT_DP_LIST)
{
	
	unsigned int i,hat_f, hat_f_left, hat_f_right, hat_a;
#pragma omp parallel shared(mask) private(i,hat_f, hat_f_left, hat_f_right,\
        hat_a) num_threads(THREAD_NUM)
    {
#pragma omp for
    for (i = 0; i < N_SAMPLE; i++) {
       mask[i]=5000; 
    }

#pragma omp for
	for (i = 0; i < DDS_NUMBER_OF_DECODERS; i++){
		if(DDS_PTT_DP_LIST[i].detect_state != DDS_INSERT_FREQ_NONE){
			hat_f = DDS_PTT_DP_LIST[i].freq_idx;
			hat_a = DDS_PTT_DP_LIST[i].freq_amp;

			hat_f_left = (hat_f<52)? (hat_f+2047-52):hat_f;
			hat_f_right = (hat_f>2047-52)? (hat_f+52-2047):hat_f;

#pragma omp for
			for (i = hat_f_left-52; i <= hat_f_right+52; i++){
				if(abs(i-hat_f)<=26 && mask[i]<2*hat_a){
                    //represents a band of 3.2kHz
					mask[i] = 2*hat_a;
				}else if(abs(i-hat_f)<=38 && mask[i]<hat_a/6){ 
					mask[i] = hat_a/6;
				}else if(abs(i-hat_f)<=52 && mask[i]<3*hat_a/32){
					mask[i] = 3*hat_a/32;
				}

			}			
			
		}
	}
    }
}

unsigned int DDS_Detection_Loop(float complex *signal, unsigned int *prevIdx, unsigned int *nPrevIdx,
                                    DDS_FreqsRecord_Typedef *DDS_PTT_DP_LIST)
{

	int i, currIdx = 0;
    /*int lower_limit = 0, upper_limit = 0;*/
	unsigned int ret_value = DDS_INSERT_FREQ_NONE, isInPrevIdx;
	unsigned int peakAmp = 1, currAmp = 0, iPass=0, peakPos, iPrevIdx;
    int mask[N_SAMPLE];
    //divisibiNlity fixed

	/*unsigned int tmp0, peakIdx = 0, aux_abs, insertedFreqs[DDS_NUMBER_OF_DECODERS];*/
	unsigned int nPass=0, assigned_decoder = 0;
    /*save the window of signal in freq*/
    DDS_PassSet_Typedef passSet;

	/*Number_of_detected_indexes()
	* In this loop, passSet stores the uFFT response.
	* can find the signal frequency through the fft index 
	* through the equation: f = idx*fs/L, where fs is sampling frequency and
	* M is the length of FFT.
	*/

	//TODO resetPassSet();

    fft(signal,N_SAMPLE);
    calc_mask(mask,DDS_PTT_DP_LIST);

    /* Compare fft amplitude with mask */
    for(i = 0; i<N_SAMPLE; i++){
        /*printf("abs signal: %f, mask: %d\n", cabs(signal[i]), mask[i]);*/
        if(cabs(signal[i])>mask[i]){
            /*printf("abs signal: %f, mask: %d\n", cabs(signal[i]), mask[i]);*/
            passSet.passIdx[nPass] = i;
            passSet.passAmp[nPass] = cabs(signal[i]);
            nPass++;
        }
    }
    printf("nPass: %d\n",nPass);

    while(peakAmp != 0 && assigned_decoder != DDS_INSERT_FREQ_NONE){

        assigned_decoder = DDS_INSERT_FREQ_NONE;
        peakAmp = 0;

        //Loop for find decoder free
#pragma omp parallel for num_threads(THREAD_NUM)
        for (i = 0; i < DDS_NUMBER_OF_DECODERS; i++){
            if(DDS_PTT_DP_LIST[i].detect_state == DDS_INSERT_FREQ_NONE){
                printf("Decoder free %d\n",i);
                assigned_decoder=i;
                break;
            }
        }

        //If anyone decoder is available:
        /*while(assigned_decoder != DDS_INSERT_FREQ_NONE && iPass<nPass){*/
        while(assigned_decoder != DDS_INSERT_FREQ_NONE && iPass<nPass){
            currIdx = (int) passSet.passIdx[iPass];
            currAmp = passSet.passAmp[iPass];
            if (TEST_SIGN_BIT(currIdx, DDS_FREQ_NUMBER_OF_BITS)) {
                currIdx = CONVERT_TO_32BIT_SIGNED(currIdx,
                DDS_FREQ_NUMBER_OF_BITS); // sign extent
                // now currIdx is in 2's complement 32bit
            }
            printf("currIdx: %d - currAmp: %d, iPass: %d\n",passSet.passIdx[iPass],passSet.passAmp[iPass],iPass);
            //first amplitude test, find the highest amplitude signal.
            if (currAmp > peakAmp) {
                isInPrevIdx = 0;
                //Test if signal is present in two consecutive windows
                //(double detection criteria)
                //The loop scan the signal vector of the previous window 
                for (iPrevIdx = 0; iPrevIdx < *nPrevIdx; iPrevIdx++){
                    if(currIdx==prevIdx[iPrevIdx]){ //doubleDETECTION
                        isInPrevIdx = 1;
                        break;
                    }
                }
                //Assign the signal that passes as a peak
                if(isInPrevIdx){
                    //FREQ_DETECTED_TWICE;
                    peakAmp = currAmp;
                    //peakIdx = currIdx;
                    peakPos = iPass;
                } 
            }
            iPass++;
        }
     //Update Detected PttDpList
        if(peakAmp>0){
            DDS_PTT_DP_LIST[assigned_decoder].freq_idx
                = passSet.passIdx[peakPos];
            DDS_PTT_DP_LIST[assigned_decoder].freq_amp = peakAmp;
            DDS_PTT_DP_LIST[assigned_decoder].detect_state = FREQ_DETECTED_TWICE;
            //Update passSet
            passSet.passAmp[peakPos]=0;
            passSet.passIdx[peakPos]=0;
            ret_value = 1;
        }
    } //while

    //Update prevPassIdx
    for (iPass=0; iPass < nPass; iPass++){
        currIdx = (int) passSet.passIdx[iPass];
        if (TEST_SIGN_BIT(currIdx, DDS_FREQ_NUMBER_OF_BITS)) {
            currIdx = CONVERT_TO_32BIT_SIGNED(currIdx,
            DDS_FREQ_NUMBER_OF_BITS); // sign extent
        }
        prevIdx[iPass]=currIdx;
        /*printf("prevIdx: %d, passSet.passIdx: %d\n",prevIdx[iPass],passSet.passIdx[iPass]);*/
    }

    //The length of previous indexes in next window is length of current window
    *nPrevIdx = nPass; 
    return ret_value;
}

