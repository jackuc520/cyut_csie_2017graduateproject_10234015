#include<msp430.h>
#include<stdio.h>
#include <stdbool.h>
#define   Num_of_Results   8
#define DT 250

unsigned int ADC_value=0;
void ConfigureAdc(void);
void signal_processing(void);
volatile unsigned int results[Num_of_Results];

bool LInput=false;
bool FirstlessOPT=false;//開始小於1.3V
bool newbpm=false;
bool tenthaft=false;

struct fuct{
  int voltage;    
  int index;
}peakvalue[10];

// Global variables
int BPMavg=0;
int adc[10] = {0}; //Sets up an array of 10 integers and zero's the values
int avg_adc = 0;

void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;		// Stop WDT
  BCSCTL1 = CALBC1_1MHZ;		// Set range   DCOCTL = CALDCO_1MHZ;
  BCSCTL2 &= ~(DIVS_3);			// SMCLK = DCO = 1MHz   //user guide P.284
  P1SEL |= BIT0;			// ADC input pin P1.0
  
  //**************************************************************************************************
  //7seg setting //p1.4~1.7 control ; p2.0~2.3 data 
  P1DIR  |= 0xF2;
  P1OUT  =  0x00;
  P2DIR  |= 0x0F;
  P2OUT  =  0x00;
  //**************************************************************************************************
  
  
  //**************************************************************************************************
  //timer setting
  //P1DIR |= 0x02;                            // P1.0 output
  CCTL0 = CCIE;                             // CCR0 interrupt enabled
  CCR0 = 8050;  //8050
  TACTL = TASSEL_2 + MC_2;                  // SMCLK, contmode
  //**************************************************************************************************
    
  ConfigureAdc();			// ADC set-up function call
  __enable_interrupt();			// Enable interrupts.
  __bis_SR_register(GIE);	// Low Power Mode 0 with interrupts enabled
}
 
// Function containing ADC set-up
void ConfigureAdc(void)
{
  ADC10CTL1 = INCH_0 + ADC10DIV_3 ;         // Channel 0, ADC10CLK/3
  ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE;  // Vcc & Vss as reference, Sample and hold for 64 Clock cycles, ADC on, ADC interrupt enable
  ADC10AE0 |= BIT0;                         // ADC input enable P1.3
}

// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
  ADC10CTL0 |= ENC + ADC10SC;		// Sampling and conversion start
  P1OUT ^= 0x02;                           // Toggle P1.0
  CCR0 += 8050;                            // Add Offset to CCR
}

// ADC10 interrupt service routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void)
{
  //ADC_value = ADC10MEM;		// Assigns the value held in ADC10MEM to the integer called ADC_value
  //__bic_SR_register_on_exit(CPUOFF);        // Return to active mode }
  signal_processing();
}

void signal_processing(void)
{
    //adc processing 
  int D4,D3,D2,D1;  int i,j,nonzero=0,BPMtot=0,clear_arr=0;
  float sum;
  static unsigned int A=0,PtoP=0,BPM=0,BPMavg=0,tenthcount=0, oldBPM=0;
  static unsigned int index = 0,bpm_arr[10]={0},arr_idx=0; 
  
  if(ADC10MEM>0x155&&LInput==false){//大於1.1V
    index++;
    LInput=true;
    FirstlessOPT=true;
     if(ADC10MEM>peakvalue[0].voltage){
       peakvalue[0].index=index;
       peakvalue[0].voltage=ADC10MEM;
    }
  }
  
  else if(ADC10MEM>0x0D9&&LInput==true){//大於0.7V
     index++; 
     if(ADC10MEM>peakvalue[0].voltage){
       peakvalue[0].index=index;
       peakvalue[0].voltage=ADC10MEM;
    }
  }
  
  else if(ADC10MEM>0x020){//0.15V<voltage<0.7V
    index++;
    
    if(FirstlessOPT==true){//第一次小於0.7
      PtoP=A+peakvalue[0].index;
      A=index-peakvalue[0].index;//0.7-peak的時間
      index=0;
      peakvalue[0].index=0;
      peakvalue[0].voltage=0x000;
      //
      FirstlessOPT=false;
      LInput=false;
      newbpm=true;
    }
    
  }
  else{index++;}//小於0.3 
  
  
   //輸出BPM
    sum=(float)PtoP/100;
    sum=1/sum;
    BPM=(int)(60*sum);
    
    
   //*******************************************************************  //HR buffer setting
   //前十個BPM(陣列滿前，有可能不是前十個)用正常值比較,接下來用20%比較

    if(oldBPM!=BPM){//是新的BPM才執行
      oldBPM=BPM;
      
      if(oldBPM>35&&oldBPM<160&&tenthaft==false){          
          //決定判斷方法
         tenthcount++;
         if(tenthcount==10){
           tenthaft=true;
         }
         
         //put into array
         bpm_arr[tenthcount-1]=oldBPM;
         
         
        //**********************************************************************************  
        //如果前五個BPM值在20%誤差以上就把五個值全部丟掉(1vs2 2vs3 3vs4 4vs5)
         
         if(tenthcount>=2&&tenthcount<=5){ //要有二個以上值才開始比較, 有五個值以上就不比較
           
           if(bpm_arr[tenthcount-1]>bpm_arr[tenthcount-2]*0.8
		   &&bpm_arr[tenthcount-1]<bpm_arr[tenthcount-2]*1.2){}//最新值跟前一值比較
           
           else{//超出20%誤差，清除陣列值
             while(clear_arr<=tenthcount-1){
               bpm_arr[clear_arr]=0;
               clear_arr++;
             }
             tenthcount=0;
           }
         }
        
        //**********************************************************************************

         
          //BPM avg setting
          for(j=0;j<10;j++){
            if(bpm_arr[j]!=0){//如果不是空值就讓divider++
              nonzero++;
              BPMtot+=bpm_arr[j];
            }
          }
          
          BPMavg=BPMtot/nonzero;
         
     }
   
    else if(oldBPM>0.7*BPMavg&&oldBPM<BPMavg*1.3&&tenthaft==true){
      
      //取代陣列最舊值
      bpm_arr[arr_idx++]=oldBPM;
      
      if(arr_idx==10){
        arr_idx=0;
      }
      //BPM avg setting
      for(j=0;j<10;j++){
        BPMtot+=bpm_arr[j];
      }
          
      BPMavg=BPMtot/10;

    }
    
    //oldBPM在範圍以外
    else{}
    
   }
   
   //七段處理   
   
   if(tenthaft==true){  //開始正常顯示七段值
    D4 =(BPMavg /1000)%10;      
    P2OUT = D4;
    P1OUT = 0x80;
    
    for(i = 0 ;i<DT;i++);
    
    D3 = (BPMavg /100)%10;
    P2OUT = D3;
    P1OUT = 0x40;
    
    for(i = 0 ;i<DT;i++);
    
    D2 = (BPMavg /10)%10;
    P2OUT = D2;
    P1OUT = 0x20; 
    
    for(i = 0 ;i<DT;i++);
    
    D1 = BPMavg %10;
    P2OUT = D1;
    P1OUT = 0x10;
    
    for(i = 0 ;i<DT;i++);
  }
  
   else{       //顯示ㄈ
    P2OUT = 10;
    P1OUT = 0x80;
    for(i = 0 ;i<DT;i++);

    P2OUT = 10;
    P1OUT = 0x40;
    for(i = 0 ;i<DT;i++);

    P2OUT = 10;
    P1OUT = 0x20;
    for(i = 0 ;i<DT;i++);

    P2OUT = 10;
    P1OUT = 0x10;
    for(i = 0 ;i<DT;i++);

   }
}
