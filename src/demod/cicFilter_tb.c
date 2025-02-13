#include "../service.h"
#include "cicFilterCplxStep.h"
#define NUMBER_OF_SAMPLES 640
#define NOUT	NUMBER_OF_SAMPLES/deciRate
#define delayIdx 8
#define deciRate 20
#define smplPerSymb 8
#define inputSeqW 160
#define length inputSeqW/deciRate
int main(int argc, char *argv[]){
   int i0,i1;
   FILE* inputFile = fopen("test_files/inputCIC.txt","r");
   if(inputFile == NULL){
       printf("Can't open file!\n");
       return 1;
   }
   char input[30];
   int inputRe[NUMBER_OF_SAMPLES];
   int inputIm[NUMBER_OF_SAMPLES];
   for (i0=0;fgets(input,sizeof(input), inputFile) != NULL;i0++){
       sscanf(input,"%d %d",&inputRe[i0], &inputIm[i0]);
   }
   fclose(inputFile);
   printf("-------> Finish Load Input Signal <-------\n\n");		
   /********************************************************************************/
   int previousAccRe;
   int previousAccIm;
   int accRe, accIm, accDlyIdx;
   float complex cplxMultSignal[] = {[0 ... smplPerSymb*deciRate-1] = 0+0*I};
   float complex  demodSignal[] = {[0 ... length-1] = 0};
   float complex outputSignal[] = {[0 ... NUMBER_OF_SAMPLES/deciRate] = 0+0*I};
   mem_cic * str = malloc(sizeof(mem_cic));
		str->previousAccRe = malloc(delayIdx*sizeof(int));
		str->previousAccIm = malloc(delayIdx*sizeof(int));
		str->accRe = 0;
   str->accIm = 0;    
   str->accDlyIdx = delayIdx-1;		
   for (i0 = 0; i0<delayIdx;i0++){
     str->previousAccRe[i0]=0;
     str->previousAccIm[i0]=0;
   }
   int complex OutBuffer[] = {[0 ... NOUT] = 0};		
   printf("----------------> Test <------------------\n\n");
	
   for (i0=0;i0<NUMBER_OF_SAMPLES/(smplPerSymb*deciRate);i0++){
       for(i1=0;i1<inputSeqW;i1++){
           cplxMultSignal[i1] = inputRe[smplPerSymb*deciRate*i0+i1]+inputIm[smplPerSymb*deciRate*i
       }
       cicFilterCplxStep(cplxMultSignal, str, demodSignal,deciRate, delayIdx,inputSeqW);
       for(i1=0;i1<smplPerSymb;i1++){
					outputSignal[i0*smplPerSymb+i1] = demodSignal[i1];
       }
   }
	
   printf("---------------> Checker <----------------\n\n");
   FILE* outputFile = fopen("test_files/outputCIC.txt","r");
   if(outputFile == NULL){
     printf("Can't open file!\n");
     return 1;
   }
   char output[30];
   float outRe[NOUT];
   float outIm[NOUT];
		int nErr = 0;
   for (i0=0;fgets(output,sizeof(output), outputFile) != NULL;i0++){
       sscanf(output,"%f %f",&outRe[i0], &outIm[i0]);
			
   }
   fclose(outputFile);
   // Compare output file with outputSignal from code		
		for(i0 = 0;i0<NOUT;i0++ ){
			if(((creal(outputSignal[i0]))-outRe[i0]>3) || ((int)(cimag(outputSignal[i0]))-outIm[i0])>3){ 
				nErr++;
				printf("~~~~~ Error %d ~~~~~\n",i0);
				printf("Got: %f %f \n",creal(outputSignal[i0]),cimag(outputSignal[i0]));
				printf("Expected: %f %f \n",outRe[i0],outIm[i0]);				
			}
		}
		if(nErr != 0){
			printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
			printf("~~~~~~~~ Try Again! %d mismatches ~~~~~~~~\n",nErr);
			printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
		}else{
			printf("***-----------------------------------****\n");
			printf("***----------- SUCCESS !! ------------****\n");
			printf("***-----------------------------------****\n");
		}
		free(str->previousAccRe);
		free(str->previousAccIm);
		free(str);		
   return 0;
