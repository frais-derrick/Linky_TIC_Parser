///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//      _______ _____ _____   _____        _____   _____ ______ _____  
//     |__   __|_   _/ ____| |  __ \ /\   |  __ \ / ____|  ____|  __ \ 
//        | |    | || |      | |__) /  \  | |__) | (___ | |__  | |__) |
//        | |    | || |      |  ___/ /\ \ |  _  / \___ \|  __| |  _  / 
//        | |   _| || |____  | |  / ____ \| | \ \ ____) | |____| | \ \ 
//        |_|  |_____\_____| |_| /_/    \_\_|  \_\_____/|______|_|  \_\
//
//  Author        : Fred CHABANNE
//  Date          : 18/02/2024
//  Version       : 1.0
//  Brief         : Linky frame parser
//  
//  Test Platform : PICO PI W
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define STX 0x02
#define ETX 0x03
#define LF  0x0A
#define CR  0x0D
//#define SEP 0x09    /// <= !!! LINKY DOC : SEP is 0x09
#define SEP 0x20      /// <= !!! REAL DATA SEPARATOR


#define BUFFSIZE    1023       
#define FRAME_SIZE  255

////////////////////////////////////////////////////////////
//
//  TIC         <STX> < FRAME > <ETX>
//  FRAME       <SUBFRAME> <SUBFRAME> ... <SUBFRAME>  
//  SUBFRAME    <LF>  <LABEL> <SEP> <DATA> <SEP> <CHECKSUM> <CR>
//
////////////////////////////////////////////////////////////

/// <summary>
/// CLASS CIRCULAR BUFFER
/// </summary>
class CircularBuffer
{
public:
    CircularBuffer();
    ~CircularBuffer();

    void Push(char  ch);
    int  Get (char* ch);

private:
    char* WRPTR = 0;
    char* RDPTR = 0;

    char  receptionBuffer[BUFFSIZE + 1];

    char* SPTR = receptionBuffer;
    char* EPTR = receptionBuffer + BUFFSIZE;

};

CircularBuffer::CircularBuffer()
{
    WRPTR = receptionBuffer;
    RDPTR = receptionBuffer;
    SPTR  = receptionBuffer;
    EPTR  = receptionBuffer + BUFFSIZE;

    memset(receptionBuffer, 0, BUFFSIZE + 1);
}

CircularBuffer::~CircularBuffer()
{
}

void CircularBuffer::Push(char ch)
{
    *WRPTR = ch;
    if (WRPTR == EPTR) { 
        WRPTR = SPTR; 
    }
    else { 
        WRPTR++; 
    }
}

int CircularBuffer::Get(char* ch)
{
    if (RDPTR == WRPTR) {
        *ch = 0;
        return 0;
    }
    else {

        if (RDPTR == EPTR) {
            RDPTR = SPTR;           
        }
        else {
            RDPTR++;
        }

        *ch = *RDPTR;
        return 1;
    }
}


/// <summary>
/// CLASS TIC FRAME
/// </summary>
class TicFrame
{
public:
    TicFrame();
    ~TicFrame();

    int frameCheck(CircularBuffer &cb);

    int getInfos(long& papp, long& pinst);

private:
    int extractInfos();
    int ExtractLabel(const char* LABEL);

private:
    char  frame[FRAME_SIZE + 1];
    int   iframe;
    int   validity;

    char    DATA[32];
    long    PAPP;
    long    IINST;
    long    HCHP;
    long    HCHC;
};

TicFrame::TicFrame()
{
    validity = 0;
    iframe   = 0;
    memset(frame, 0, FRAME_SIZE + 1);
}

TicFrame::~TicFrame()
{
}

int TicFrame::getInfos(long& papp, long& pinst)
{
    papp = PAPP;
    pinst = IINST;
    // to parse new frame
    validity = 0;

    Serial.printf("PAPP = %i  IINST = %i  HC = %i  HP = %i\r\n", papp, pinst, HCHC, HCHP);

    return validity;
}

int TicFrame::ExtractLabel( const char *LABEL )
{
    int ret = 0;
    if (LABEL) {
        /// START FRAME WITH KEYWORD "LABEL"
        if (strstr(frame, LABEL) != NULL) {
            char* startSubFrame = strstr(frame, LABEL);
            if (startSubFrame) {                
                /// KEYWORD FOUND
                char* endSubFrame = strchr(startSubFrame, CR);
                if (endSubFrame) {
                    *endSubFrame = '\0';

                    /// CHECKSUM CALCULATION... not working
                    /*
                    char checksum = 0;
                    char* ptcs = startSubFrame;
                    //printf("\r\nCHECHSUM :");
                    while (ptcs != (endSubFrame-1)) {
                        //printf("%02X ", *ptcs);
                        checksum+= *ptcs;
                        ptcs++;
                    }
                    checksum = (checksum & 0x3F)+0x20;
                    char realcs = *ptcs;

                    if (checksum != realcs) {
                        int wcs = 1;
                        //printf("WRONG CHECKSUM");
                    }
                    */

                    char* tab1 = strchr(startSubFrame, SEP);
                    if (tab1) {
                        /// FIRST SEPARATOR FOUND
                        char *tab2 = strchr((tab1+1), SEP);
                        if (tab2) {
                            /// SECOND SEPARATOR FOUND
                            char* dat = (tab1 + 1);
                            char* edat= strchr((tab1 + 1), SEP);
                            *edat = '\0';
                            strcpy(DATA, dat);
                            *edat = SEP;
                            ret = 1;
                        }
                    }
                    *endSubFrame = CR;
                }
            }
        }
        else {
          /*
          Serial.print("E1: label ");
          Serial.print(LABEL);
          Serial.println(" not found");
          */
        }
    }
    return ret;
}

int TicFrame::extractInfos()
{
    int ok = 0;
    int nbInf = 0;
    int correctInf = 0;

    nbInf++;
    if (ExtractLabel("PAPP"))  {
        PAPP = atol(DATA);
        correctInf++;
    }
    nbInf++;
    if (ExtractLabel("IINST")) {
        IINST = atol(DATA);
        correctInf++;
    }
    nbInf++;
    if (ExtractLabel("HCHC")) {
        HCHC = atol(DATA);
        correctInf++;
    }
    nbInf++;
    if (ExtractLabel("HCHP")) {
        HCHP = atol(DATA);
        correctInf++;
    }

    if (nbInf && (nbInf == correctInf))
        ok = 1;
    return ok;
}

int TicFrame::frameCheck(CircularBuffer &cb)
{
    char byteChar;
 
    /// while valid char
    while (cb.Get(&byteChar))
    {
        /// Error
        if (iframe >= FRAME_SIZE) {
            iframe = 0;
        }

        /// START FRAME
        if (byteChar == STX) {
            iframe = 0;
            frame[iframe] = byteChar;
            iframe++;
        }
        /// STOP FRAME
        else if (byteChar == ETX) {
            frame[iframe] = byteChar;
            iframe++;
            frame[iframe] = '\0';
            validity = extractInfos();            
            iframe = 0;
        }
        /// ANOTHER CHAR
        else {
            if (iframe > 0) {
                frame[iframe] = byteChar;
                iframe++;
            }
        }
    }

    return validity;
}

///////////////////////////////////////////////////////////////////////////////////////////
//
//UART Interface	  TX GPIOs	                  RX GPIOs
//UART0	            GPIO0, GPIO12, GPIO16	      GPIO1, GPIO13, GPIO17
//UART1	            GPIO4, GPIO8	              GPIO5, GPIO9
//
///////////////////////////////////////////////////////////////////////////////////////////

int RXpin  = 17;
int TXpin  = 16;
int baud   = 1200;
int config = SERIAL_7E1;

//#define UNIT_TEST

#ifndef UNIT_TEST
int incomingByte = 0; // for incoming serial data

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(250);
  Serial.println("TIC extractor v1.0");

  Serial1.setRX(RXpin);
  Serial1.setTX(TXpin);
  Serial1.begin(baud, config);

}

long mypapp, mypinst;
CircularBuffer  cb;
TicFrame        tf;

void loop() {
  // put your main code here, to run repeatedly:
  while (Serial1.available() > 0) {
    // read the incoming byte:
    incomingByte = Serial1.read();
    // debug : transmit received
    //Serial.write(incomingByte);

    cb.Push(incomingByte);
    if (tf.frameCheck(cb))
        tf.getInfos(mypapp, mypinst); 

  }   
}

#else

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Serial line operational !!");    
}


void loop() {

    // emulated TIC frame :
    /*
    002078240 "
    PTEC TH.. $
    IINST 000 W
    IMAX 090 H
    PAPP 00070 (
    HHPHC A ,
    MOTDETAT 000000 B
    ADCO xxxxxxxxxxxx O
    OPTARIF BASE 0
    ISOUSC 30 9
    BASE 002078240 "
    PTEC TH.. $
    IINST 000 W
    IMAX 090 H    
    */

  /// LINKY DOC FRAME (SEP = 0x09)
    /*char sampleFrame1[1024] = "002078240\t\"\r\x03\x02\nPTEC\tTH..\t$\r\nIINST\t002\tY\r\nIMAX\t090\tH\r\nPAPP\t00620\t)\r\nHHPHC\tA\t,\r\nHCHC\t006421882\t%\r\nHCHP\t014615540\t-\r\nMOTDETAT\t000000\tB\r\nADCO\txxxxxxxxxxxx\tO\r\nOPTARIF\tBASE\t0\r\nISOUSC\t30\t9\r\nBASE\t002078240\t\"\r\x03\x02\nPTEC\tTH..\t$\r\nIINST\t002\tY\r\nIMAX\t090\tH\r\nPAPP\t00630\t*\r\nHHPHC\tA\t,\r\nHCHC\t006421882\t%\r\nHCHP\t014615540\t-\r\nMOTDETAT\t000000\tB\r\nADCO\txxxxxxxxxxxx\tO\r\nOPTARIF\tBASE\t0\r\nISOUSC\t30\t9\r\nBASE\t002078240\t\"\r\x03\x02\nPTEC\tTH..\t$\r\nIINST\t003\tW\r\nIMAX\t090\tH\0";
    */
  /// REAL LINKY FRAME (SEP = SPACE)  
    char sampleFrame1[1024] = "002078240 \"\r\x03\x02\nPTEC TH.. $\r\nIINST 002 Y\r\nIMAX 090 H\r\nPAPP 00620 )\r\nHHPHC A ,\r\nHCHC 006421882 %\r\nHCHP 014615540 -\r\nMOTDETAT 000000 B\r\nADCO xxxxxxxxxxxx O\r\nOPTARIF BASE 0\r\nISOUSC 30 9\r\nBASE 002078240 \"\r\x03\x02\nPTEC TH.. $\r\nIINST 002 Y\r\nIMAX 090 H\r\nPAPP 00630 *\r\nHHPHC A ,\r\nHCHC 006421882 %\r\nHCHP 014615540 -\r\nMOTDETAT 000000 B\r\nADCO xxxxxxxxxxxx O\r\nOPTARIF BASE 0\r\nISOUSC 30 9\r\nBASE 002078240 \"\r\x03\x02\nPTEC TH.. $\r\nIINST 003 W\r\nIMAX 090 H\0";
    int len = strlen(sampleFrame1);    
    Serial.printf("sampleFrame1 lenght = %i\r\n", len);

   
    long mypapp, mypinst;
    CircularBuffer  cb;
    TicFrame        tf;


    for (int k = 0; k < 300; k++)
    {
        int idx;

        for (idx = 0; idx < len; idx++)
        {
            cb.Push(sampleFrame1[idx]);
            if (tf.frameCheck(cb))
                tf.getInfos(mypapp, mypinst);
        }  
    }


    delay(1000);

}

#endif
