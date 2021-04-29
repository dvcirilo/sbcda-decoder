#include "Gap8.h" 
#include "rt/rt_api.h"
#include "decoder.h"
#include "inputSignalMin.h"
#include "../lib/kiss_fft.h"
#include "../lib/_kiss_fft_guts.h"
#include "service.h"
#define NUMBER_OF_SAMPLES 47360 
#define TRIGGER (17)

//#define DETECT_DEBUG
//#define DEBUG_DEMOD
static void mureceiver(void *arg)
{
  //GPIO Setup
  rt_padframe_profile_t *profile_gpio = rt_pad_profile_get("hyper_gpio");
  if(profile_gpio == NULL){
	  printf("pad config error\n");
//	  return 1;
  }
  rt_padframe_set(profile_gpio);

  //GPIO Init
  rt_gpio_init(0,TRIGGER);
  //Config TRIGGER pin as an output
  rt_gpio_set_dir(0,1<<TRIGGER, RT_GPIO_IS_OUT);
  rt_gpio_set_pin_value(0,TRIGGER,1);

 //rt_freq_set(RT_FREQ_DOMAIN_FC,200000000);
 rt_freq_set(RT_FREQ_DOMAIN_CL,  175000000);
  int n0;
 // printf("--------> Loading Input Signal %d <-------\n\n",rt_freq_get(RT_FREQ_DOMAIN_CL));
 //int complex input [1280*36];
 // printf("--------> Finish Load Input Signal <-------\n\n");   
   
  //sampler memory
  int i;
  
  sampler_mem * str_smp[NUMBER_OF_DECODERS];
  for (i = 0; i < NUMBER_OF_DECODERS; ++i){
    str_smp[i] = rt_alloc(MEM_ALLOC,sizeof(sampler_mem));
    str_smp[i]->smplBuffer = rt_alloc(MEM_ALLOC,2*smplPerSymb*(sizeof(int)));
  }

  //CIC filter of demod
  mem_cic * str_cic[NUMBER_OF_DECODERS];
  for (i = 0; i < NUMBER_OF_DECODERS; ++i){
    str_cic[i] = rt_alloc(MEM_ALLOC,sizeof(mem_cic));
    str_cic[i]->previousAccRe = rt_alloc(MEM_ALLOC,delayIdx*sizeof(int));
    str_cic[i]->previousAccIm = rt_alloc(MEM_ALLOC,delayIdx*sizeof(int));
  }
  
  //CIC filter of sampler
  mem_cic * str_cicSmp[NUMBER_OF_DECODERS];
  for (i = 0; i < NUMBER_OF_DECODERS; ++i){
    str_cicSmp[i]= rt_alloc(MEM_ALLOC,sizeof(mem_cic));
    str_cicSmp[i]->previousAccRe = rt_alloc(MEM_ALLOC,delaySmp*sizeof(int));
    str_cicSmp[i]->previousAccIm = rt_alloc(MEM_ALLOC,delaySmp*sizeof(int));
  }

  //demod_mem stores accumulators, symbOut and symbLock
  demod_mem * str_demod[NUMBER_OF_DECODERS];
  for(i=0;i<NUMBER_OF_DECODERS;i++){
    str_demod[i] = rt_alloc(MEM_ALLOC,sizeof(demod_mem));
    str_demod[i]->symbLock = rt_alloc(MEM_ALLOC,nSymb*sizeof(int));
    str_demod[i]->symbOut = rt_alloc(MEM_ALLOC,nSymb*sizeof(int));
  }

  // This struct is used to control the FSM and show bit results
  PTTPackage_Typedef * wpckg[NUMBER_OF_DECODERS];
  for(i=0;i<NUMBER_OF_DECODERS;i++){
    wpckg[i] = rt_alloc(MEM_ALLOC,sizeof(PTTPackage_Typedef));
  }

  // struct stores the interface (detect to demod) parameters
  FreqsRecord_Typedef *PTT_DP_LIST[NUMBER_OF_DECODERS];
  for(i=0;i<NUMBER_OF_DECODERS;i++){
    PTT_DP_LIST[i] = rt_alloc(MEM_ALLOC,sizeof(FreqsRecord_Typedef));
  }

  int * prevIdx = rt_alloc(MEM_ALLOC,DFT_LENGTH*sizeof(int));
  for(i=0;i<DFT_LENGTH;i++){
	  prevIdx[i]=0;
  }

//  printf("***------------ ALL READY --------------***\n");
  int complex *inputSignal = rt_alloc(MEM_ALLOC,WINDOW_LENGTH*sizeof(int complex));
  int tmp0=0,f=0,iCh,vga, activeList, nWind, spWind,iSymb,i0,i2;
  int * vgaExp = rt_alloc(MEM_ALLOC,NUMBER_OF_DECODERS*sizeof(int));
  int * vgaMant = rt_alloc(MEM_ALLOC,NUMBER_OF_DECODERS*sizeof(int));
  int * InitFreq = rt_alloc(MEM_ALLOC,NUMBER_OF_DECODERS*sizeof(int));
  int detect_time=0, demod_time=0, decod_time=0, total_time=0,aux_time, decod_per_channel=0;
#ifdef DEBUG_DEMOD
  int debug = 0;
#endif

/*
//Calculate LUT for fft computation---------------------------*
cpx * we = rt_alloc(MEM_ALLOC,1024*sizeof(cpx));//-*
for(i=0;i<DFT_LENGTH/2;i++){//				     -*
	we[i].r=(cos(2*PI*i/DFT_LENGTH));//    		     -*
	we[i].i=-1*(sin(2*PI*i/DFT_LENGTH));//               -*
}//-----------------------------------------------------------*
*/

//  kiss_fft_cfg cfg;
//  cfg = kiss_fft_alloc(DFT_LENGTH,0,NULL,NULL);

//#ifndef DETECT_DEBUG
  for(iCh=0;iCh<NUMBER_OF_DECODERS;iCh++){
   // printf("Clearing decoder %d\n",iCh);
    clearDecoder(PTT_DP_LIST[iCh],wpckg[iCh], str_cic[iCh], str_cicSmp[iCh], str_smp[iCh], str_demod[iCh]);
  }
//#endif
/*
#ifdef DETECT_DEBUG
 for(iCh=0;iCh<NUMBER_OF_DECODERS;iCh++){
	PTT_DP_LIST[iCh]->detect_state = FREQ_NONE;
	PTT_DP_LIST[iCh]->freq_amp = 0;
	PTT_DP_LIST[iCh]->freq_idx = 0;
	PTT_DP_LIST[iCh]->timeout = 0;	
 }
#endif
*/
#define NSIM (1)
  
for(n0=0; n0<NSIM;n0++){
//printf("N_SIM = %d\n",n0);
#ifdef DEBUG_DEMOD
  if(n0==1)
   debug = 1;
#endif



for (nWind=0;nWind<NUMBER_OF_SAMPLES/WINDOW_LENGTH;nWind++){
//  aux_time = rt_time_get_us();
//  for (nWind=0;nWind<2;nWind++){ //DEBUG DETECT PROCESS
    //printf("***--------- Window processing ---------*** [%d]\n", nWind);
    
    // Performs input partitioning on the windows of 1280 samples
    memcpy(inputSignal,inputSignalMin+WINDOW_LENGTH*nWind,1280*sizeof(int complex));
    
//    rt_gpio_set_pin_value(0,TRIGGER,1);
    
    /* DEBUG SIGNAL WINDOW
    if(nWind>30){
	    printf("0 %d %d\n",inputSignal[0]);
	    printf("L %d %d\n",inputSignal[WINDOW_LENGTH-1]);
    }*/

   //printf("Updating Timeout !\n");

   UpdateTimeout(PTT_DP_LIST,wpckg);
#ifndef DEBUG_DEMOD
   //detect_time = rt_time_get_us();

//   printf("Starting detection loop ! %d us\n", detect_time);
   tmp0 =  detectLoop(inputSignal, prevIdx, PTT_DP_LIST);   
   //detect_time = rt_time_get_us()-detect_time;
  //  rt_gpio_set_pin_value(0,TRIGGER,0);
  // printf("Detect: %d us\n",detect_time);
   //DEBUG Purpose
   if(tmp0){
   //  printf("New PTT(s) detected!\nStatus of all decoders:\n");
     for(iCh=0;iCh<NUMBER_OF_DECODERS;iCh++){
   //    printf("ch:%d state: %d freq: %4d amp: %4d\n",iCh,PTT_DP_LIST[iCh]->detect_state, PTT_DP_LIST[iCh]->freq_idx, PTT_DP_LIST[iCh]->freq_amp);
     }
     tmp0=0;
   }
    //Setup Parameters: Frequency, Gain, Controls status of pckg and Detect.
    for (iCh=0;iCh<1;iCh++){
      if(PTT_DP_LIST[iCh]->detect_state==FREQ_DETECTED_TWICE){
        vga = VgaGain(PTT_DP_LIST[iCh]->freq_amp);
        vgaExp[iCh] = -1*(vga&0x3F);
        vgaMant[iCh] = (vga>>6)&0xFF;
        InitFreq[iCh] = PTT_DP_LIST[iCh]->freq_idx<<9;
//       printf("[%d]: mant %d exp %d freq %d\n",iCh, vgaMant[iCh],vgaExp[iCh],PTT_DP_LIST[iCh]->freq_idx);
        PTT_DP_LIST[iCh]->detect_state=FREQ_DECODING;
        wpckg[iCh]->status=PTT_FRAME_SYNCH;
      }
    }
#endif

#ifdef DEBUG_DEMOD
   // DEBUG DEMOD AND DECOD PROCESSES  
    if(nWind==1 && debug==0){
	PTT_DP_LIST[0]->detect_state = FREQ_DECODING;
	PTT_DP_LIST[0]->freq_amp = 793;
	PTT_DP_LIST[0]->freq_idx = 1788;
	PTT_DP_LIST[0]->timeout = 100;
	vga = VgaGain(PTT_DP_LIST[0]->freq_amp);
	vgaExp[0] = -1*(vga&0x3F);
	vgaMant[0] = (vga>>6)&0xFF;
	InitFreq[0] = PTT_DP_LIST[0]->freq_idx<<9;
 //       printf("[%d]: mant %d exp %d\n",0, vgaMant[0],vgaExp[0]);
        wpckg[0]->status=PTT_FRAME_SYNCH;
	PTT_DP_LIST[1]->detect_state = FREQ_DECODING;
	PTT_DP_LIST[1]->freq_amp = 787;
	PTT_DP_LIST[1]->freq_idx = 397;
	PTT_DP_LIST[1]->timeout = 100;
	vga = VgaGain(PTT_DP_LIST[1]->freq_amp);
	vgaExp[1] = -1*(vga&0x3F);
	vgaMant[1] = (vga>>6)&0xFF;
	InitFreq[1] = PTT_DP_LIST[1]->freq_idx<<9;
        //printf("[%d]: mant %d exp %d\n",0, vgaMant[1],vgaExp[1]);
        wpckg[1]->status=PTT_FRAME_SYNCH;
    }
#endif

//#ifndef DETECT_DEBUG
    //decodes signals from active channels
    for (iCh=0;iCh<NUMBER_OF_DECODERS;iCh++){
      if(PTT_DP_LIST[iCh]->detect_state==FREQ_DECODING){
//      printf("Starting demod process channel %d\n",iCh);
//      demod_time = rt_time_get_us();
	decod_per_channel = rt_time_get_us();
        pttA2Demod(inputSignal, InitFreq[iCh], vgaMant[iCh], vgaExp[iCh], str_demod[iCh], str_cic[iCh], str_cicSmp[iCh], str_smp[iCh]);

  //      demod_time = rt_time_get_us()-demod_time;
//	printf("Demod time: %d ms \n",demod_time/1000);
        for(iSymb = 0;iSymb<nSymb;iSymb++){
            if(str_demod[iCh]->symbLock[iSymb]){
//		    printf("smb %d",str_demod[iCh]->symbOut[i1]);
            wpckg[iCh]->total_symbol_cnt++;
            if(wpckg[iCh]->status==PTT_FRAME_SYNCH){
              frameSynch(wpckg[iCh],str_demod[iCh]->symbOut[iSymb]);   
            }else if(wpckg[iCh]->status==PTT_DATA){
              readData(wpckg[iCh],str_demod[iCh]->symbOut[iSymb]);
              if(wpckg[iCh]->status==PTT_READY){
                //fill the package and clear the decoder
/*		printf("ready!\n");
                printf("|%d|\n",iCh);
                for(i2=0;i2<wpckg[iCh]->msgByteLength;i2++){
                  printf("%x\n",wpckg[iCh]->userMsg[i2]);
                }
                printf("Clearing decoder %d\n",iCh);
*/
                clearDecoder(PTT_DP_LIST[iCh],wpckg[iCh], str_cic[iCh], str_cicSmp[iCh], str_smp[iCh], str_demod[iCh]);
                //DEBUG Purpose                
              }
            }else if(wpckg[iCh]->status==PTT_ERROR){
              printf("Clearing decoder %d\n",iCh);
              clearDecoder(PTT_DP_LIST[iCh],wpckg[iCh], str_cic[iCh], str_cicSmp[iCh], str_smp[iCh], str_demod[iCh]);
            }
          }
        }
     	decod_per_channel= rt_time_get_us()-decod_per_channel;
//	printf("decod_time %d: %d us\n",iCh,decod_per_channel/1000);
      }
//	rt_gpio_set_pin_value(0,TRIGGER,0);
    }//END FOR SCROLLING CHANNELS
  // rt_gpio_set_pin_value(0,TRIGGER,0);
//#endif

  }//END FOR SCROLLING WINDOWS  
  
//  rt_gpio_set_pin_value(0,TRIGGER,0);
/*  printf("result!\n");
  printf("|0|\n");
  for(i2=0;i2<wpckg[0]->msgByteLength;i2++){
     printf("%x\n",wpckg[0]->userMsg[i2]);
  }
*/
  //total_time = rt_time_get_us()-aux_time;
  //printf("total time %d\n",total_time/1000);
  }//END FOR NSIM 
  rt_gpio_set_pin_value(0,TRIGGER,0);

  rt_free(MEM_ALLOC,prevIdx,DFT_LENGTH*sizeof(int));
  rt_free(MEM_ALLOC,inputSignal,WINDOW_LENGTH*sizeof(int complex));
  rt_free(MEM_ALLOC,vgaExp,NUMBER_OF_DECODERS*sizeof(int));
  rt_free(MEM_ALLOC,vgaMant,NUMBER_OF_DECODERS*sizeof(int));
  rt_free(MEM_ALLOC,InitFreq,NUMBER_OF_DECODERS*sizeof(int));
  
  for(i=0;i<NUMBER_OF_DECODERS;i++){
    rt_free(MEM_ALLOC,PTT_DP_LIST[i],sizeof(FreqsRecord_Typedef));
    rt_free(MEM_ALLOC,wpckg[i],sizeof(PTTPackage_Typedef));
    
#ifndef DETECT_DEBUG
    rt_free(MEM_ALLOC,str_smp[i]->smplBuffer,2*smplPerSymb*(sizeof(int)));   
    rt_free(MEM_ALLOC,str_smp[i],sizeof(sampler_mem));       
    rt_free(MEM_ALLOC,str_cicSmp[i]->previousAccRe,delaySmp*sizeof(int));
    rt_free(MEM_ALLOC,str_cicSmp[i]->previousAccIm,delaySmp*sizeof(int));
    rt_free(MEM_ALLOC,str_cicSmp[i],sizeof(mem_cic));
    rt_free(MEM_ALLOC,str_cic[i]->previousAccRe,delayIdx*sizeof(int));
    rt_free(MEM_ALLOC,str_cic[i]->previousAccIm,delayIdx*sizeof(int));
    rt_free(MEM_ALLOC,str_cic[i],sizeof(mem_cic));        
    rt_free(MEM_ALLOC,str_demod[i]->symbLock,nSymb*sizeof(int));
    rt_free(MEM_ALLOC,str_demod[i]->symbOut,nSymb*sizeof(int));
    rt_free(MEM_ALLOC,str_demod[i],sizeof(demod_mem));    
#endif
  }

 // printf("---------------> Check <-------------------\n\n");  

}

#define STACK_SIZE	4000
#define MOUNT           1
#define UNMOUNT         0
#define CID             0
unsigned int done = 0;
static int alloc_size = 512;
static void hello(void *arg)
{
  printf("[clusterID: 0x%2x] Hello from core %d\n", rt_cluster_id(), rt_core_id());
}

static void free_mem(void* p, int size, rt_free_req_t* req)
{
rt_free_cluster(MEM_ALLOC, p, alloc_size, req);
rt_free_cluster_wait(req);
}
static void cluster_entry(void *arg)
{
 // printf("Entering cluster on core %d\n", rt_core_id());
 // printf("There are %d cores available here.\n", rt_nb_pe());
  rt_team_fork(1, mureceiver, (void *)0x0);
 // printf("Leaving cluster on core %d\n", rt_core_id());
}

static void end_of_call(void *arg)
{
//  printf("[clusterID: 0x%x] MUR from core %d\n", rt_cluster_id(), rt_core_id());
  done = 1;
}

int main()
{
//  rt_event_sched_t * p_sched = rt_event_internal_sched();
  
  rt_freq_set(RT_FREQ_DOMAIN_FC,250000000);
  printf("Entering main controller \n");
  int t_time = rt_time_get_us();  

  if (rt_event_alloc(NULL, 8)) return -1;

  rt_event_t *p_event = rt_event_get(NULL, end_of_call, (void *) CID);

  rt_cluster_mount(MOUNT, CID, 0, NULL);

  void *stacks = rt_alloc(MEM_ALLOC, STACK_SIZE/**rt_nb_pe()*/);
  if(stacks == NULL) return -1;

  rt_cluster_call(NULL, CID, cluster_entry, NULL, stacks, STACK_SIZE, 0,1, p_event);

  while(!done){
    rt_event_execute(NULL, 1);
  }

  rt_cluster_mount(UNMOUNT, CID, 0, NULL);
  t_time = rt_time_get_us()-t_time;
  printf("total time: %d ms\n",t_time/1000);

  printf("Test success: Leaving main controller\n");
  return 0;
}
