////////////////////////////////////////////////
//
// Copper SFP/SFP+ PHY register check tool
//      by morikawa@wavesplitter.com 2023/12/21
//
///////////////////////////////////////////////
#include	<stdio.h>
#include	<string.h>
#include	<stdint.h>
#include	<time.h>
#include	<linux/i2c.h>
#include	<sys/ioctl.h>
#include	<wiringPi.h>
#include	<wiringPiI2C.h>

#define	I2C_SMBUS	0x0720
#define	I2C_SMBUS_READ	1
#define I2C_SMBUS_WRITE	0

#define	I2C_SMBUS_BYTE	1
#define I2C_SMBUS_BYTE_DATA 2
#define I2C_SMBUS_WORD_DATA	3
#define	I2C_SMBUS_BLOCK_MAX	32

int fd;
int mdio_type;

int nGet_ASCII(int addr, int len, char* str){

	int i,j;
	j = 0;
	for (i = addr; j < len; i++){
		str[j++] = wiringPiI2CReadReg8(fd,i);
	}
	str[j] = 0x00;
	return j;
}
int byte_order(int i){
	return ((i & 0x00ff) * 0x100 + (i & 0xff00) / 0x100);
}
// MDIO access via I2C MIDO bridge
// 0xac >> 1 = 0x56
// MDIO_OFFSET = 0x21(DEVID + 0x20)
//
// union i2c_smbus_data{
//	uint8_t byte;
//	uint16_t word;
//	uint8_t block[I2C_SMBUS_BLOCK_MAX +2 ];
//};
struct i2c_smbus_ioctl_data{
	char read_write;
	uint8_t command;
	int size;
	union i2c_smbus_data *data;
};
static inline int i2c_smbus_access(int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data){
	struct i2c_smbus_ioctl_data args;
	args.read_write = rw;
	args.command = command;
	args.size = size;
	args.data = data;
	return ioctl(fd, I2C_SMBUS, &args);
};
int wiringPiI2CRead16(int fd){
	union i2c_smbus_data data;
	if(i2c_smbus_access (fd, I2C_SMBUS_READ, 0 , 3, &data))
	return -1;
	else return (data.word);
}
//
//  Clause 45
//
int CL45_Read(int devid, int addr){
	fd = wiringPiI2CSetup(0x56);
	delay(2);	// wait 2ms (> 1ms)
	wiringPiI2CWriteReg16(fd, (devid + 0x20), byte_order(addr));
	delay(2);
//	return (wiringPiI2CReadReg16(fd,0x20));
//	return (wiringPiI2CRead(fd) << 8) + wiringPiI2CRead(fd);
	return byte_order(wiringPiI2CRead16(fd));
}
//
// i2cget 1 0x56 reg w
//
int CL22_Read(int devid, int reg){
	// 22 + 0x40 = 0x56
        fd = wiringPiI2CSetup(0x56);
	return(byte_order(wiringPiI2CReadReg16(fd, reg)));
}
//
// read RollBall type
//
int RB_Read(int devid, int reg){
	fd = wiringPiI2CSetup(0x51);
	wiringPiI2CWriteReg8(fd, 0x7b, 0xff);	// write password 
	wiringPiI2CWriteReg8(fd, 0x7c, 0xff);
	wiringPiI2CWriteReg8(fd, 0x7d, 0xff);
	wiringPiI2CWriteReg8(fd, 0x7e, 0xff);
	wiringPiI2CWriteReg8(fd, 0x7f, 0x03);	// select page 03

	wiringPiI2CWriteReg8(fd, 0x81, 0x01);	// DEVADDR
	wiringPiI2CWriteReg16(fd, 0x82, byte_order(reg));
	wiringPiI2CWriteReg8(fd, 0x80, 0x02);	// MDIO read command
	delay(100);				// wait 100ms
//	fprintf(stderr,"RB status = %4d\n", wiringPiI2CReadReg8(fd, 0x80));
//	wiringPiI2CReadReg8(fd, 0x80);		// check done status

	return byte_order(wiringPiI2CReadReg16(fd, 0x84));
}
int PHY_Read(int devid, int reg){
	int ret;
	switch(mdio_type){
	case 0:	ret = 0; break;
	case 1: ret = CL22_Read(devid, reg); break;
	case 2: ret = CL45_Read(devid, reg); break;
	case 3: ret = RB_Read(devid, reg); break;
	}
	return ret;
}
//
//	for broadcom 84891L(PHY ID 0x3590 5080/5081
//
// Decice ID 0x0002/0x0003
// PMA/PMD status 0x0001
//  FAULT bit7
//  RCV_LINK-STATUS bit2
//  CAP LOW POPOWER
int Status(int data){
	printf("Addr 0x0001(0x0002)=%04x : ", data);

	if(data & 0x0080) printf("Fault condition/");
	if(data & 0x0004) printf("MPA/PMD link up/");
	if(data & 0x0002) printf("Low power mode");
	printf("\n");
	return(data);
}
int Control(int data){
	int data2;
	printf("Addr 0x0000(0x0000)=%04x : ", data);

	if(data & 0x8000) printf("PMA/PMD reset\n");
	data2 = data & 0x2040;
	if(data2 == 0x2000) printf("speed select 1000M\n");
	if(data2 == 0x0040) printf("speed select 100M\n");
	if(data & 0x0800) printf("low power mode\n");
	if(data2 == 0x2040){	// bit 6 and 13
		data2 = data & 0x0003c;	// mask 111100
		if(data2 == 0x001c) printf("speed select 5Gbps\n");	// 0111
		if(data2 == 0x0018) printf("speed select 2.5Gbps\n");	// 0110
		if(data2 == 0x0014) printf("speed select 400Gbps\n");	// 0101
		if(data2 == 0x0010) printf("speed select 25Gbps\n");	// 0100
		if(data2 == 0x000c) printf("speed select 100Gbps\n");	// 0011
		if(data2 == 0x0008) printf("speed select 40Gbps\n");	// 0010
		if(data2 == 0x0004) printf("speed select 10PASSS-T/n");	// 0001
		if(data2 == 0x0000) printf("speed select 10Gbps\n");	// 0000
	}	
	return(data);
}
//
// PMA/PMD speed ability DEVAD =1, address = 0x0004
//
int SpeedAb(int data){
	printf("Addr 0x0004(0x40b1)=%04x :", data);

	if(data & 0x8000);		// bit 15 ignore on read
	if(data & 0x4000) printf("CAP5G/");
	if(data & 0x2000) printf("CAP2p5G/");
	if(data & 0x1000) ;		// bit 12

	if(data & 0x0800) ;		// bit 11
	if(data & 0x0400) printf("CAP_40G_100G/");
	if(data & 0x0200) printf("CAP_P2MP/");
	if(data & 0x0100) ;		// bit 8

	if(data & 0x0080) printf("CAP_100B_TX/");
	if(data & 0x0040) printf("CAP_10GB_KX/");
	if(data & 0x0020) printf("CAP_100M/");
	if(data & 0x0010) printf("CAP_1000M/");

	if(data & 0x0008);		// bit 3 reserved
	if(data & 0x0004) printf("CAP_10PASS_TS/");
	if(data & 0x0002) printf("CAP_2BASE_TL/");
	if(data & 0x0001) printf("CAP_10G");

	printf("\n");
	return(data);
}
//
// IEEE Fast retrain 1,0x93 is broadcom/marvell common
//
int FastR(int data){
	printf("1-0x93:Fast Retrain = %04x\n",data);
	printf("Fast Retrain count RX=%2d/TX=%2d\n", data >>11, (data & 0x07c0)>> 6);	// bit 15:11 and 10:6
	if(data & 0x0010) printf("Fast retrain supported\n");
	else			printf("Fast retrain not supported\n");
	if(data & 0x0008) printf("Fast retrain was negosiated\n");
	else			printf("Fast retrain not negosiated\n");
	if(data & 0x0001) printf("Fast retrain capability is enables\n");
	else			printf("Fast retrain capability is disabled\n");
	return data;
}
int BCM84891_LED(int data1, int data2){
	printf("LED status %04x %04x\n",data1,data2);
	if(data1 & 0x0080) printf("10G link is up\n");
	if((data1 & 0x0018) == 0x0018) printf("1G link up\n");
	if((data1 & 0x0018) == 0x0010) printf("100M link up\n");
	if(data2 & 0x0008) printf("5G link up\n");
	if(data2 & 0x0004) printf("2.5G link up\n");
}
//
//
// Firmware revition and DATE
//
int BCM84891_FwR(int data1, int data2){
	int bu = (data1 & 0xf000) >> 12;
	int ma = (data1 & 0x0f80) >> 7;
	int br = (data1 & 0x007f);
	int mon = (data2 & 0x1e00) >> 9;
	int day = (data2 & 0x01f0 ) >> 4;
	int year = (data2 & 0x000f) + ((data2 & 0x2000) >> 9);
	printf("FW %04x %04x Ver %0d2.%02d(%02d) DATE %02d/%02d/%02d\n",data1,data2, ma, br, bu, year, mon, day);
}
//
// DEVAD +7, address 0x0000 - 0x0005
//
int AN_Regs(){
	int data;

	data = CL45_Read(7,0x000);
	printf("10G-T AN status reg : %04x:",data);
        data = CL45_Read(7, 0x0100);
        printf(" %04x:",data);
        wiringPiI2CWriteReg16(fd,0x27,0x0200);
        data = wiringPiI2CReadReg16(fd, 0x20);
        printf(" %04x:",data);
        wiringPiI2CWriteReg16(fd,0x27,0x0300);
        printf(" %04x:",data);
        wiringPiI2CWriteReg16(fd,0x27,0x0400);
        data = wiringPiI2CReadReg16(fd, 0x20);
        printf(" %04x:",data);

	printf("\n");
}
//
//	MARVELL 88X33XX
//
int M88X33_SignalD(int data){
	printf("10G PMA/PMD Signal Detect = %04x\n", data);
	return data;
}
int tx_pwr_bit(int i){
	int v;
	switch(i){
	case 0x00: v=0; break;
	case 0x01: v=2; break;
	case 0x02: v=4;	break;
	case 0x03: v=6; break;
	case 0x04: v=8; break;
	case 0x05: v=10; break;
	case 0x06: v=12; break;
	case 0x07: v=16; break;
	default: v=0; break;
	}
	return v;
}
// common TX power
int TxP(int data){
	printf("TX pwr level %04x  %3d dB/%3d dB",data,tx_pwr_bit(data>>13), tx_pwr_bit((data & 0x1c00)>>10));
	if(data & 0x0001) printf(":short reach mode\n");
	else printf("\n");
	return data;
}
//
int M88X33_Fw(int data1,int data2){
	printf("Firmware %04x%04x\n",data1,data2);
	
}
// Copper Specific status register Device 3, Register 0x8008
// 
int M88X33_CuS(int data){
	int data2;
	printf("Copper status %04x\n",data);
	data2 = data & 0xc00c;			// bit 2,3,14,15
	if(data & 0x0400) {
	printf("link up with ");
	switch(data2){
	case 0xc000:	printf("10G");	break;
	case 0x8000:	printf("1000M");	break;
	case 0x4000:	printf("100M");	break;
	case 0x0000:	printf("10M");	break;
	case 0x0008:	printf("2.5G");	break;
	case 0x000c:	printf("5G");	break;
	default:	break;
	}
	if(data & 0x0040) 	printf(" MDIX\n");
	else 			printf(" MDI\n");
   }	// if link up
}
// 30,0x400d
int BCM84891_CuState(int data){
	printf("Copper status = %04x",data);
	if(data & 0x0002) printf(" Copper detect");
	else 		  printf(" Copper not detect");
	if(data & 0x0020) {
	printf(" Link up with ");
        switch((data & 0x001c)){
        case 0x18:    printf("10G");  break;
        case 0x10:    printf("1000M");        break;
        case 0x08:    printf("100M"); break;
//      case 0x00:    printf("10M");  break;
        case 0x04:    printf("2.5G"); break;
        case 0x0c:    printf("5G");   break;
        default:        break;
        }
     }
	printf("\n");

}
//
//  main MAIN main MAIN
//
int main(){
	char PN[32];
	char SN[32];
	char DATE[32];
	uint32_t PHYID,PHYID_CL22,PHYID_CL45, PHYID_RB;

	const int offset =0;	// for SFP SFF-8472
	fprintf(stderr, "Copper SFP/SFP+ PHY register check tool\n");

	wiringPiSetupGpio();
	pinMode(4, INPUT);

	fd = wiringPiI2CSetup(0x50);	// check A0h page 00 get BASIC info
	nGet_ASCII(40+offset, 16 , PN);
	nGet_ASCII(68+offset, 16, SN);

//////////

	PHYID_CL22 = CL22_Read(1, 0x0002) * 0x10000 + CL22_Read(1, 0x0003);
	PHYID_CL45 = CL45_Read(1, 0x0002) * 0x10000 +
		CL45_Read(1, 0x0003);
	PHYID_RB = RB_Read(1, 0x0002) * 0x1000 + RB_Read(1, 0x0003);

	printf("PN = %16s\n", PN);
	printf("SN = %16s\n", SN);
	printf("PHY ID = [CL22]%08x/[CL45]%08x/[RB]%08x\n", PHYID_CL22, PHYID_CL45,PHYID_RB);

	switch(PHYID_RB & 0xffff0000){
		case 0x35900000: mdio_type = 3;PHYID = PHYID_RB;
			printf("=== MDIO type select RollBall\n");	
			 break;
		case 0x03590000: mdio_type = 3;PHYID = PHYID_RB;
			printf("=== MDIO type select RollBall\n");
			break;
		case 0x00020000:
		case 0x002b0000: mdio_type = 3;PHYID = PHYID_RB; 
			printf("=== MDIO type select RollBall\n");
			break;
		default: mdio_type = 2;PHYID = PHYID_CL45;
			printf("=== MDIO type select Clause 45\n");
			 break;
	}
// standard register
	Control(PHY_Read(1, 0x0000));
	Status(PHY_Read(1, 0x0001));	
	SpeedAb(PHY_Read(1, 0x0004));

//
	switch(PHYID){
	case 0x03595081:
	case 0x35905080:	// DEV_ID A0
	case 0x35905081:	// DEV_ID B0
		printf("=== PHY Broadcom BCM84891L\n");	
		FastR(PHY_Read(1, 0x0093));
//		BCM84891_FastRC(CL45_Read(1, 0xa89f));
		TxP(PHY_Read(1, 0x0083));
		BCM84891_CuState(PHY_Read(30, 0x400d));
		BCM84891_FwR(PHY_Read(30, 0x400f), PHY_Read(30, 0x4010));
		break;
	case 0x600d8542:
		printf("=== PHY Broadcom Any\n");
		TxP(CL45_Read(1, 0x0083));
                BCM84891_CuState(CL45_Read(30, 0x400d));
		BCM84891_FwR(PHY_Read(30, 0x400f), PHY_Read(30, 0x4010));
		break;
	case 0x600d8500:
		printf("=== PHY BCM84328\n");
		break;
	case 0xae025150:
		printf("=== PHY Broadcom BCM84881\n");
		break;
	case 0x31c31c12:
		printf("=== PHY AQR113\n");
		break;
	case 0x0002b9ab:
	case 0x002b09a0:
	case 0x002b09ab:
		printf("=== PHY MARVELL 88X3310\n");
		TxP(CL45_Read(1, 0x83));
		M88X33_CuS(PHY_Read(3, 0x8008));
		M88X33_Fw(PHY_Read(1, 0xc011),PHY_Read(1, 0xc012));
	default: break;
	}
}
