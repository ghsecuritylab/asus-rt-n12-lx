/*
 *  P2P  support ; WIFI-DRIECT
 *
 *  $Id: 8192cd_p2p.c,v 1.1 2011/05/16 09:59:22 edison_shih Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_P2P_C_


#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/kernel.h>
#endif

#include <linux/random.h>
#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_hw.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"
//#include "./8192cd_util.h"
#include "./8192cd_p2p.h"

extern UINT8 WSC_IE_OUI[];

typedef int  p2pcmd_func_t(struct rtl8192cd_priv *priv, unsigned char *data);

struct p2p_cmd_list {
	unsigned char cmd[32];
	p2pcmd_func_t* p2p_cmd_func;
};


/*--------------debug---------------------*/


/*---------------function declare start------------------ */
char* p2p_get_token(char *, char *);
unsigned char*	p2p_search_tag(unsigned char *,  int ,unsigned char , int *);
unsigned char* p2p_check_tag(unsigned char *, 	int , unsigned char , int *);


int P2P_scan(struct rtl8192cd_priv *priv, unsigned char *data);
int P2P_search(struct rtl8192cd_priv *priv, unsigned char *data);
int P2P_listen(struct rtl8192cd_priv* priv,unsigned char* data);
void p2p_show_ss_res(struct rtl8192cd_priv *priv);
int P2P_show_status(struct rtl8192cd_priv *priv, unsigned char *data);

int p2pcmd_enable(struct rtl8192cd_priv *, unsigned char *);
int p2pcmd_discovery(struct rtl8192cd_priv *priv, unsigned char *data);
int p2pcmd_find(struct rtl8192cd_priv *priv, unsigned char *data);
int p2pcmd_set(struct rtl8192cd_priv *priv, unsigned char *data);
int p2pcmd_set_listen_channel(struct rtl8192cd_priv *priv, unsigned char *data);

int p2pcmd_force_GO(struct rtl8192cd_priv *priv, unsigned char *data);
int p2pcmd_backtoDev(struct rtl8192cd_priv *priv, unsigned char *data);
int p2p_as_GO(struct rtl8192cd_priv *priv, int GOtype);
int p2p_as_preClient(struct rtl8192cd_priv *priv);



int wsc_build_probe_rsp_ie(struct rtl8192cd_priv *priv ,
	unsigned char *data , unsigned short DEVICE_PASSWORD_ID);

int p2p_build_probe_req_ie(struct rtl8192cd_priv *priv, unsigned char *data);



void indicate_wscd(struct rtl8192cd_priv *priv , unsigned char mode ,
	unsigned char *PSK ,struct p2p_device_peer *current_nego_peer);

void p2p_on_provision_req(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo);
int p2p_issue_provision_rsp(struct rtl8192cd_priv *priv, unsigned char *da);

int p2p_issue_provision_req(struct rtl8192cd_priv *priv );
void p2p_on_provision_rsp(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo);

void p2p_on_GO_nego_req(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo);

int p2p_issue_GO_nego_rsp(struct rtl8192cd_priv *priv,
		struct p2p_device_peer *current_nego_peer);
void p2p_on_GO_nego_conf(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo);
int p2p_issue_GO_nego_req(struct rtl8192cd_priv *priv);

void p2p_on_GO_nego_rsp(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo);
int p2p_issue_GO_nego_conf(struct rtl8192cd_priv *priv,
		struct p2p_device_peer *current_nego_peer);



/*---------------function declare end-------------------- */

/*---------------local var declare start------------------ */

unsigned char P2P_WILDCARD_SSID[8]="DIRECT-";

unsigned char WFA_OUI[WFA_OUI_LEN] = {0x50, 0x6F, 0x9A};
unsigned char WFA_OUI_PLUS_TYPE[WFA_OUI_PLUS_TYPE_LEN] = {0x50, 0x6F, 0x9A , 0x9};

unsigned char WILDCARD_ADDR[MAC_LEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

/*---------------local var declare end------------------ */


#define BAND_CONTROL	0	// no meaning


/*make sure i am under 2G band when p2p listen mode */
void stay_on_2G(struct rtl8192cd_priv *priv)
{
	unsigned long flags ;
	if (priv->pmib->dot11RFEntry.phyBandSelect != PHY_BAND_2G)
	{	
		P2P_DEBUG("	Switch to 2G Band\n");
		SAVE_INT_AND_CLI(flags);
		//SMP_LOCK(flags);
		
		PHY_SetBBReg(priv, rFPGA0_AnalogParameter4, 0x00f00000, 0xf);
		priv->pmib->dot11RFEntry.phyBandSelect = PHY_BAND_2G;
		priv->pmib->dot11BssType.net_work_type = (WIRELESS_11B|WIRELESS_11G|WIRELESS_11N);
		UpdateBBRFVal8192DE(priv);
		PHY_SetBBReg(priv, rFPGA0_AnalogParameter4, 0x00f00000, 0x0);

		RESTORE_INT(flags);
	   // SMP_UNLOCK(flags);	

	}

	return;

}


void stay_on_5G(struct rtl8192cd_priv *priv)
{
	unsigned long flags ;
	if (priv->pmib->dot11RFEntry.phyBandSelect != PHY_BAND_5G)
	{
		P2P_DEBUG("	Switch to 5G Band\n");
		SAVE_INT_AND_CLI(flags);
		//SMP_LOCK(flags);		
		// stop BB
		PHY_SetBBReg(priv, rFPGA0_AnalogParameter4, 0x00f00000, 0xf);
		priv->pmib->dot11RFEntry.phyBandSelect = PHY_BAND_5G;
		priv->pmib->dot11BssType.net_work_type = (WIRELESS_11A|WIRELESS_11N);
		UpdateBBRFVal8192DE(priv);
		PHY_SetBBReg(priv, rFPGA0_AnalogParameter4, 0x00f00000, 0x0);

		RESTORE_INT(flags);
	   // SMP_UNLOCK(flags);			
	}
	return;
}







#define UTILITY_FUNCTIONS	0	// no meaning


/*--------------------------------
*---------------------------------
*
*	utility unctions area		 
*
*---------------------------------
---------------------------------*/

void generate_random_xy(unsigned char *data,int len)
{
	char *String09azAZ="0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	unsigned char byteList[30];
	int idx;	
	if(len>30)
		return;
	
	get_random_bytes((void *)&byteList, len);		
	for(idx=0;idx<len;idx++){
		data[idx] = String09azAZ[byteList[idx]%62];
	}
	data[len]='\0';
	//P2P_DEBUG("return %s\n" ,data);

}

void init_channel_list(struct rtl8192cd_priv *priv)
{

	int idx = 0;

	memset(&priv->p2pPtr->my_channel_list,0,sizeof(struct p2p_channels));

	memcpy(priv->p2pPtr->my_channel_list.country ,"US " ,3);

	priv->p2pPtr->my_channel_list.reg_class_mun=3;
	
	priv->p2pPtr->my_channel_list.reg_class[0].reg_class = 81;
	priv->p2pPtr->my_channel_list.reg_class[0].channel_mun=11;
	for(idx=0;idx<11;idx++)
		priv->p2pPtr->my_channel_list.reg_class[0].channel[idx]= idx+1;

	priv->p2pPtr->my_channel_list.reg_class[1].reg_class = 115;
	priv->p2pPtr->my_channel_list.reg_class[1].channel_mun=4;
	for(idx=0;idx<4;idx++)
		priv->p2pPtr->my_channel_list.reg_class[1].channel[idx]= 36 + idx*4;

	priv->p2pPtr->my_channel_list.reg_class[2].reg_class = 124;
	priv->p2pPtr->my_channel_list.reg_class[2].channel_mun = 5;
	for(idx=0;idx<5;idx++)
		priv->p2pPtr->my_channel_list.reg_class[2].channel[idx]= 149 + idx*4;

	priv->p2pPtr->my_channel_list.Id11_len = 3+(2+11)+(2+4)+(2+5);
	//P2P_DEBUG("channel list len=%d\n",priv->p2pPtr->my_channel_list.Id11_len);
}

void generate_GO_ssid(struct rtl8192cd_priv *priv)

{

	unsigned char tmpstr[24];

	generate_random_xy(tmpstr,2);

	memcpy(priv->p2pPtr->ssid_random ,tmpstr ,2);

	memcpy(priv->p2pPtr->my_GO_ssid,P2P_WILDCARD_SSID,P2P_WILDCARD_SSID_LEN);
	
	memcpy(&priv->p2pPtr->my_GO_ssid[P2P_WILDCARD_SSID_LEN] ,priv->p2pPtr->ssid_random,2);

#if 0	// add postfix ssid after -
	priv->p2pPtr->my_GO_ssid[P2P_WILDCARD_SSID_LEN+2]='-';
	convert_bin_to_str(GET_MY_HWADDR, 6, tmpstr);		
	memcpy(priv->p2pPtr->my_GO_ssid+P2P_WILDCARD_SSID_LEN+2+1 ,tmpstr,12);
	priv->p2pPtr->my_GO_ssid[P2P_WILDCARD_SSID_LEN+2+1+12]='\0';
#else	
	priv->p2pPtr->my_GO_ssid[P2P_WILDCARD_SSID_LEN+2]='\0';
	priv->p2pPtr->my_GO_ssid_len = strlen(priv->p2pPtr->my_GO_ssid);
#endif	
	P2P_DEBUG("GO SSID=%s\n",priv->p2pPtr->my_GO_ssid);
	
}	



void generate_GO_PSK(struct rtl8192cd_priv *priv)

{
	unsigned char tmpstr[65];
	generate_random_xy(tmpstr,8);
	memcpy(priv->p2pPtr->go_PSK ,tmpstr ,8);
	priv->p2pPtr->go_PSK[8]='\0';	
	P2P_DEBUG("GO PSK=%s\n",priv->p2pPtr->go_PSK);	
}	

	
/*
 *  find a token in a string. If succes, return pointer of token next. If fail, return null
 */
char* p2p_get_token(char *data, char *token)
{
		int idx=0, src_len=strlen(data), token_len=strlen(token);

		while (src_len >= token_len) {
			if (!memcmp(&data[idx], token, token_len)){
				if(data[idx+token_len]==','|| data[idx+token_len]=='=')
					return (&data[idx+token_len+1]);
				else
					return (&data[idx+token_len]);
			}
			src_len--;
			idx++;
		}
		return NULL;
}


char *get_config_token(char *data, char *token)
{
	char *ptr=data;
	int len=0, idx=0;

	while (*ptr && *ptr != '\n' ) {
		if (*ptr == '=') {
			if (len <= 1)
				return NULL;
			memcpy(token, data, len);

			/* delete ending space */
			for (idx=len-1; idx>=0; idx--) {
				if (token[idx] !=  ' ')
					break;
			}
			token[idx+1] = '\0';

			return ptr+1;
		}
		len++;
		ptr++;
	}
	return NULL;
}


int get_config_value(char *data, char *value)
{
	char *ptr=data;	
	int len=0, idx, i;

	while (*ptr && *ptr != '\n' && *ptr != '\r') {
		len++;
		ptr++;
	}

	/* delete leading space */
	idx = 0;
	while (len-idx > 0) {
		if (data[idx] != ' ') 
			break;	
		idx++;
	}
	len -= idx;

	/* delete bracing '"' */
	if (data[idx] == '"') {
		for (i=idx+len-1; i>idx; i--) {
			if (data[i] == '"') {
				idx++;
				len = i - idx;
			}
			break;
		}
	}

	if (len > 0) {
		memcpy(value, &data[idx], len);
		value[len] = '\0';
	}
	return len;
}

int is_zero_ether_addr(const unsigned char *a)
{
	return !(a[0] | a[1] | a[2] | a[3] | a[4] | a[5]);
}

int is_zero_pin_code(const unsigned char *a)
{
	return !(a[0] | a[1] | a[2] | a[3] | a[4] | a[5]| a[6]| a[7]);
}
void p2p_debug_out(unsigned char *label, unsigned char *data, int data_length)
{
    int i,j;
    int num_blocks;
    int block_remainder;

    num_blocks = data_length >> 4;
    block_remainder = data_length & 15;

	if (label)
		printk("%s\n", label);

	if (data==NULL || data_length==0)
		return;

    for (i=0; i<num_blocks; i++) {
        printk("\t");
        for (j=0; j<16; j++)
			printk("%02x ", data[j + (i<<4)]);       
		printk("\n");
    }

    if (block_remainder > 0) {
		printk("\t");
		for (j=0; j<block_remainder; j++)
            printk("%02x ", data[j+(num_blocks<<4)]);        
        printk("\n");
    }
}


/*
 * show value by binary format
 */
void ShowValByBinary(unsigned int ByteIn, char *binary_str ,int len )
{
		int idx;				
		for( idx=0 ; idx < len;idx++){
			if(ByteIn & 1<<(len - 1 - idx))
				binary_str[idx]='1';
			else
				binary_str[idx]='0';
		}
		binary_str[idx+1]='\0';
}


#define P2P_ATTRIBUTE_PARSE	0	// no meaning

/*----------------------------------------------------------------
	------------------------------------------------------------------*/


void parse_channel_list(unsigned char* pData , int tag_len ,
	struct p2p_device_peer *current_nego_peer)
{
	int idx;
	int len=tag_len;
	int channels;
	int reg_class=0;
	
	unsigned char CountryCodeStr[4];
	strncpy(CountryCodeStr , pData ,3);
	CountryCodeStr[3]='\0';
	//P2P_DEBUG("	CountryCode:%s\n", CountryCodeStr);	
	//P2P_DEBUG("	Channel:");	
	len -= 3;
	pData +=3;

	while(len){
		//P2P_DEBUG("	op class:%d\n",pData[0]);			
		channels = pData[1];
		for( idx=0 ; idx < channels ;idx++){
			if(current_nego_peer)
				current_nego_peer->channels_list.reg_class[reg_class].channel[idx]=pData[2+idx];
			//P2P_DEBUG(" 	channel:%d \n",pData[2+idx]);		
		}
		len -= (2+channels);
		pData += (2+channels);
		reg_class++;
	}


}

void parse_device_info(unsigned char* pData , int tag_len ,
	struct device_info_s* dev_info_ptr )
{
	int TagLenChk=tag_len;
	int idx = 0;
	unsigned  char NoSD=0;
	unsigned short WSC_T;
	unsigned short WSC_L;	
	unsigned char DeviceName[33];

	/*device address */
	//P2P_DEBUG("P2P Device Address:");
	//MAC_PRINT(pData);	
	if(dev_info_ptr){
		memcpy(dev_info_ptr->dev_address,pData,6 );
	}
	
	pData += 6;
	TagLenChk -= 6;

	/*config method */	
	//P2P_DEBUG("	Config method:0x%02X\n",*(unsigned short*)pData);			
	if(dev_info_ptr){
		dev_info_ptr->config_method = *(unsigned short*)pData;
	}

	
	pData += 2;
	TagLenChk -= 2;	


	/*Primary Device Type */	
	if(dev_info_ptr){
		memcpy(dev_info_ptr->pri_dev_type , pData , 8);
	}	
	
	pData	+=	8;
	TagLenChk	-=	8;

	/*Number of Second Device Type */	

	NoSD = pData[0];
	pData += 1;
	TagLenChk -= 1;

	if(dev_info_ptr)
		dev_info_ptr->sdv_mun = NoSD;
	
	/* N * Second Device Type */		
	if(NoSD){
		for(  idx=0 ; idx < NoSD ; idx++){
			if(idx < MAX_SEC_DEV_TYPE){	// now we just keep 2 set , take care
				if(dev_info_ptr)
					memcpy(&dev_info_ptr->sec_dev_type[idx] ,pData,8);
			}
			pData += 8;
			TagLenChk -= 8;			
		}
	}
	
	/* Device Name (WSC TLV(2+2+N))*/

	WSC_T = *((unsigned short *)pData);	// Tag

	pData+=2;

	WSC_L = *((unsigned short *)pData);	// LEN

	if(WSC_T != 0x1011)
		P2P_DEBUG("chk!! should be 0x1011\n");

	pData+=2;	
	strncpy(DeviceName,pData,WSC_L);		
	DeviceName[WSC_L]='\0';

	//P2P_DEBUG("Device name:%s\n",DeviceName);		
	
	if(dev_info_ptr){
		strcpy(dev_info_ptr->devname ,DeviceName);
	}



	TagLenChk -= (4+WSC_L);
	if(TagLenChk){
		P2P_DEBUG("chk!! TagLenChk(%d) should be 0\n",TagLenChk);	
		P2P_DEBUG("tag_len=%d\n",tag_len);		
	}
	
	
}


void parse_group_info(unsigned char* pData , int tag_len)
{
	int TagLenChk=tag_len;
	int EachClientInfoLen=0;	
	int idx = 0;
	unsigned  char NoSD=0;
	unsigned short WSC_T;
	unsigned short WSC_L;	
	unsigned char DeviceName[33];

	/* process  sum of all p2p clients*/
	while(TagLenChk){

		/*length of bellowing fields */
		EachClientInfoLen = pData[0];
		pData += 1;
	
		TagLenChk -= EachClientInfoLen;
			
		/*device address */
		P2P_DEBUG(" Device Addr:");
		MAC_PRINT(pData);
		

		pData +=6;	
		EachClientInfoLen -= 6;
		
		/*interface address */
		P2P_DEBUG(" interface Addr:");
		MAC_PRINT(pData);

		pData +=6;		
		EachClientInfoLen -= 6;
		
		/*config method */	
		P2P_DEBUG("Config method:0x%02X\n",*(unsigned int*)pData);			
		pData +=2;		
		EachClientInfoLen -= 2;
	
	
		/*Primary Device Type */	
		P2P_DEBUG("	Primary Device Type:category_id=0x%x, oui=%02x%02x%02x%02x, sub_category_id=0x%x\n",
		 le16_to_cpu(*((unsigned short *)pData)), pData[2],pData[3],pData[4],pData[5],
		 le16_to_cpu(*((unsigned short *)&pData[6])));
	
		pData +=8;		
		EachClientInfoLen -= 8;
		
	
		/*Number of Second Device Type */	
	
		NoSD = pData[0];
		pData +=1;		
		EachClientInfoLen -= 1;
	
		/* N * Second Device Type */		
		if(NoSD){
			for(  ; idx < NoSD ; idx++){
				P2P_DEBUG("		Second Device Type:category_id=0x%x, oui=%02x%02x%02x%02x, sub_category_id=0x%x\n",
				 le16_to_cpu(*((unsigned short *)pData)), pData[2],pData[3],pData[4],pData[5],
				 le16_to_cpu(*((unsigned short *)&pData[6])));
	
				pData +=8;		
				EachClientInfoLen -= 8;
			}
		}
		
		/* Device Name (WSC TLV(2+2+N))*/

		WSC_T = *((unsigned short *)pData);
		if(WSC_T != 0x1011)
			P2P_DEBUG("chk!! should be 0x1011\n");


		pData+=2;
		WSC_L = *((unsigned short *)pData);
		P2P_DEBUG("device name len=%d\n",WSC_L);


		pData+=2;	
		strncpy(DeviceName,pData,WSC_L);		
		DeviceName[WSC_L]='\0';

			
	
		pData += WSC_L;
		EachClientInfoLen -= (4+WSC_L);
	
		if(EachClientInfoLen)
			P2P_DEBUG("chk!! EachClientInfoLen should be 0 now\n");	

	}
	
	
	
}


void parse_group_id(unsigned char* pData , int tag_len ,
	struct p2p_device_peer *current_nego_peer)
{
	/*ID 15*/		
	unsigned char StrSSID[33];

	if(current_nego_peer)
		memcpy(current_nego_peer->group_bssid , pData ,6);
	
	pData += 6;
	tag_len -= 6 ;

	/*p2p Device Address */
	strncpy(StrSSID,pData,tag_len);		
	StrSSID[tag_len]='\0';

	if(current_nego_peer){
		strcpy(current_nego_peer->group_ssid , StrSSID);	
		current_nego_peer->group_ssid_len = strlen(current_nego_peer->group_ssid);
	}
	
}


void parse_p2p_interface(unsigned char* pData , int tag_len)
{
	
	int idx = 0;
	int AddrCnt = 0;
	
	/*p2p Device Address */
	P2P_DEBUG("	P2P DevAddr:");
	MAC_PRINT(pData);

	
	pData += 6 ;
	tag_len -= 6 ;


	/*p2p  interface addr count*/
	AddrCnt = pData[0];

	/* p2p_inter_addr_cnt * inter_addr */		
	if(AddrCnt){
		for(  ; idx < AddrCnt ; idx++){

			P2P_DEBUG("	P2P ifAddr:");
			MAC_PRINT(pData);
			
			pData += 6 ;
			tag_len -= 6 ;
		}
	}	
	
	if(tag_len){
		P2P_DEBUG("chk!! tag_len should be 0 now\n");	
	}
	
	
}	


void parse_p2p_Capability(void)
{

	P2P_PRINT("0... .... Reserved\n");
	P2P_PRINT(".0.. .....Reserved\n");
	P2P_PRINT("..1. .... Device Supports P2P Invitation Procedure\n");
	P2P_PRINT("...0 .... Device Limit, able to participate in additional P2P Groups\n");
	P2P_PRINT(".... 0... Infrastructure manamgement NOT upported\n");
	P2P_PRINT(".... .0.. Concurrent operation NOT supported\n");
	P2P_PRINT(".... ..0. Client discoverability NOT supported\n");
	P2P_PRINT(".... ...1 Service discovery supported\n\n");
	
	P2P_PRINT("0... .... Reserved\n"); 
	P2P_PRINT(".0.. .... Not operating as GO in the Provisioning phase\n");
	P2P_PRINT("..0. .... Persistent reconnect NOT supported\n");
	P2P_PRINT("...0 .... Cross connection NOT supported\n");
	P2P_PRINT(".... 0... Intra-BSS distribution NOT supported\n");
	P2P_PRINT(".... .0.. The GO is able to add additional Clients\n");
	P2P_PRINT(".... ..0. Persistent P2P Group supported\n");
	P2P_PRINT(".... ...0 NOT acting as GO\n");

}	


void p2p_cancel_noa(struct rtl8192cd_priv *priv)
{
	if (priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_NONE)
		return;

	cancel_timer2(priv);
	P2P_DEBUG("Stop all NoA action!\n");
	if (priv->p2pPtr->noa_list_t.p2p_txpause_flag) {
		RTL_W8(TXPAUSE, RTL_R8(TXPAUSE) & 0xe0);
		priv->p2pPtr->noa_list_t.p2p_txpause_flag = 0;
	}

	priv->p2pPtr->noa_list_t.go_ps_type = P2P_GO_PS_NONE;
}


void p2p_noa_timer(struct rtl8192cd_priv *priv)
{
	unsigned int cur_tsf, pre_tbtt, offset;
	//P2P_DEBUG("\n");
	if (priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_OPPPS) {
		if (priv->p2pPtr->noa_list_t.p2p_txpause_flag) {
			RTL_W8(TXPAUSE, RTL_R8(TXPAUSE) & 0xe0);
			priv->p2pPtr->noa_list_t.p2p_txpause_flag = 0;

			cur_tsf = RTL_R32(TSFTR);
			pre_tbtt = (cur_tsf / (priv->beacon_period * 1024)) * (priv->beacon_period * 1024);
			offset = ((priv->p2pPtr->noa_list_t.CTWindow_OppPs & 0x7f) - 1) * 1024;
			setup_timer2(priv, pre_tbtt + offset);
		}
		else {
			RTL_W8(TXPAUSE, RTL_R8(TXPAUSE) | 0x1f);
			priv->p2pPtr->noa_list_t.p2p_txpause_flag = 1;

			cur_tsf = RTL_R32(TSFTR);
			pre_tbtt = (cur_tsf / (priv->beacon_period * 1024)) * (priv->beacon_period * 1024);
			setup_timer2(priv, pre_tbtt + (priv->beacon_period * 1024));
		}
	}
	else if (priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_NP_NOA) {
		if (priv->p2pPtr->noa_list_t.p2p_txpause_flag) {
			RTL_W8(TXPAUSE, RTL_R8(TXPAUSE) & 0xe0);
			priv->p2pPtr->noa_list_t.p2p_txpause_flag = 0;
			priv->p2pPtr->noa_list_t.go_ps_type = P2P_GO_PS_NONE;
		}
	}
	else if (priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_CONT_NOA) {
		if (priv->p2pPtr->noa_list_t.p2p_txpause_flag) {
			RTL_W8(TXPAUSE, RTL_R8(TXPAUSE) & 0xe0);
			priv->p2pPtr->noa_list_t.p2p_txpause_flag = 0;
			offset = priv->p2pPtr->noa_list_t.noa_counter * priv->p2pPtr->noa_list_t.noa_descs.interval;
			setup_timer2(priv, priv->p2pPtr->noa_list_t.noa_descs.starttime + offset);
		}
		else {
			RTL_W8(TXPAUSE, RTL_R8(TXPAUSE) | 0x1f);
			priv->p2pPtr->noa_list_t.p2p_txpause_flag = 1;
			offset = priv->p2pPtr->noa_list_t.noa_counter * priv->p2pPtr->noa_list_t.noa_descs.interval
				+ priv->p2pPtr->noa_list_t.noa_descs.duration;
			setup_timer2(priv, priv->p2pPtr->noa_list_t.noa_descs.starttime + offset);
			priv->p2pPtr->noa_list_t.noa_counter++;
		}
	}
	else { // priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_NOA
	}
}


void p2p_process_noa(struct rtl8192cd_priv *priv)
{
	unsigned int cur_tsf, pre_tbtt, offset;

	if ((priv->p2pPtr->noa_list_t.CTWindow_OppPs & 0x80) && ((priv->p2pPtr->noa_list_t.CTWindow_OppPs & 0x7f) != 0))
		priv->p2pPtr->noa_list_t.go_ps_type = P2P_GO_PS_OPPPS;
	else if (priv->p2pPtr->noa_list_t.noa_descs.count == 1)
		priv->p2pPtr->noa_list_t.go_ps_type = P2P_GO_PS_NP_NOA;
	else if (priv->p2pPtr->noa_list_t.noa_descs.count == 255)
		priv->p2pPtr->noa_list_t.go_ps_type = P2P_GO_PS_CONT_NOA;
	else if (priv->p2pPtr->noa_list_t.noa_descs.count != 0)
		priv->p2pPtr->noa_list_t.go_ps_type = P2P_GO_PS_NOA;
	else {
		P2P_DEBUG("No NoA parameters!\n\n");
		return;
	}
	P2P_DEBUG("Start GO PS type of %d\n\n", priv->p2pPtr->noa_list_t.go_ps_type);

	if (priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_OPPPS) {
		cur_tsf = RTL_R32(TSFTR);
		pre_tbtt = (cur_tsf / (priv->beacon_period * 1024)) * (priv->beacon_period * 1024);
		offset = ((priv->p2pPtr->noa_list_t.CTWindow_OppPs & 0x7f) - 1) * 1024;
		setup_timer2(priv, pre_tbtt + offset);
		priv->p2pPtr->noa_list_t.p2p_txpause_flag = 0;
	}
	else if (priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_NP_NOA) {
		RTL_W8(TXPAUSE, RTL_R8(TXPAUSE) | 0x1f);
		priv->p2pPtr->noa_list_t.p2p_txpause_flag = 1;

		cur_tsf = RTL_R32(TSFTR);
		if (TSF_LESS(cur_tsf, priv->p2pPtr->noa_list_t.noa_descs.starttime))
			setup_timer2(priv, priv->p2pPtr->noa_list_t.noa_descs.starttime +
				priv->p2pPtr->noa_list_t.noa_descs.duration);
		else
			setup_timer2(priv, cur_tsf + priv->p2pPtr->noa_list_t.noa_descs.duration);
	}
	else if (priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_CONT_NOA) {
		cur_tsf = RTL_R32(TSFTR);
		if (TSF_LESS(cur_tsf, priv->p2pPtr->noa_list_t.noa_descs.starttime)) {
			setup_timer2(priv, priv->p2pPtr->noa_list_t.noa_descs.starttime);
			priv->p2pPtr->noa_list_t.noa_counter = 0;
		}
		else {
			RTL_W8(TXPAUSE, RTL_R8(TXPAUSE) | 0x1f);
			priv->p2pPtr->noa_list_t.p2p_txpause_flag = 1;
			setup_timer2(priv, priv->p2pPtr->noa_list_t.noa_descs.starttime +
				priv->p2pPtr->noa_list_t.noa_descs.duration);
			priv->p2pPtr->noa_list_t.noa_counter = 1;
		}
	}
	else { // priv->p2pPtr->noa_list_t.go_ps_type == P2P_GO_PS_NOA
	}
}


void parse_p2p_NOA(struct rtl8192cd_priv *priv, unsigned char* pData, int tag_len, int seq)
{
	unsigned int noa_num, u32;
	//unsigned int cur_tsf, offset;

	if (pData[0] != priv->p2pPtr->noa_list_t.index) {
		p2p_cancel_noa(priv);
		priv->p2pPtr->noa_list_t.index = pData[0];
		priv->p2pPtr->noa_list_t.CTWindow_OppPs = pData[1];

		memset(&priv->p2pPtr->noa_list_t.noa_descs, 0, sizeof(struct noa_desc));
		noa_num = (tag_len - 2) / 13;
		if (noa_num) {
			priv->p2pPtr->noa_list_t.noa_descs.count = *(pData + 2);
			memcpy(&u32, (pData + 2) + 1, 4);
			priv->p2pPtr->noa_list_t.noa_descs.duration = le32_to_cpu(u32);
			memcpy(&u32, (pData + 2) + 5, 4);
			priv->p2pPtr->noa_list_t.noa_descs.interval = le32_to_cpu(u32);
			memcpy(&u32, (pData + 2) + 9, 4);
			priv->p2pPtr->noa_list_t.noa_descs.starttime = le32_to_cpu(u32);
		}

		P2P_DEBUG("Update NoA (seq %d): index %d, OppPs 0x%02x, NoA count %d, duration %d, interval %d, start 0x%08x\n",
			seq,
			priv->p2pPtr->noa_list_t.index,
			priv->p2pPtr->noa_list_t.CTWindow_OppPs,
			priv->p2pPtr->noa_list_t.noa_descs.count,
			priv->p2pPtr->noa_list_t.noa_descs.duration,
			priv->p2pPtr->noa_list_t.noa_descs.interval,
			priv->p2pPtr->noa_list_t.noa_descs.starttime);

		p2p_process_noa(priv);
	}
}	


#define P2P_TAG_SEARCH	0	// no meaning


/*P2P TLV format: T(1B)L(2B)V(var)*/
unsigned char*	p2p_search_tag(unsigned char *data_be_search, 
	int data_len,unsigned char tag, int *out_len)
{
	unsigned char id;
	unsigned short tag_len;
	int size;

	while (data_len > 0) {
		memcpy(&id, data_be_search, 1);
		memcpy(&tag_len, data_be_search+1, 2);
		tag_len = le16_to_cpu(tag_len);
		
		if (id == tag) {
			if (data_len >= (1+2+ tag_len)) {

				*out_len = (int)tag_len;
				return (&data_be_search[3]);
			}
			else {
				P2P_DEBUG("Found tag [0x%x], but invalid length!\n", tag);
				P2P_DEBUG("data_len=%d , 1+2+tag_len(%d)\n", data_len , tag_len);
				break;
			}
		}
		size = 1+2 + tag_len;
		data_be_search += size;
		data_len -= size;
	}

	return NULL;
}



unsigned char *p2p_check_tag(unsigned char *TLV_data, 	int TLV_len, unsigned char tag, int *o_len)
{
	unsigned char *pData;
	int tag_len;
#ifdef P2P_DEBUGMSG
	unsigned char tmpstr[50];
	unsigned char tmpstr2[50];
	unsigned short tmp_u16_1;
	unsigned short tmp_u16_2;		
#endif	



	pData = p2p_search_tag(TLV_data, TLV_len,tag, &tag_len);

	if (pData == NULL) {

		return NULL;
	}else{
		P2P_DEBUG("tag(%d)	:", tag);
	}
	
#ifdef P2P_DEBUGMSG
	switch(tag){

		case TAG_STATUE :
			P2P_PRINT("	Status:%d\n",pData[0]);
			break;	
		case TAG_MINOR_RES_CODE :		
			P2P_PRINT(" Minor reason code: %d\n",pData[0]);
			break;				
		case TAG_P2P_CAPABILITY :
			ShowValByBinary(pData[0] , tmpstr , 8);
			ShowValByBinary(pData[1] , tmpstr2 , 8);			
			P2P_PRINT(" p2p Capability: (%s %s)\n",tmpstr,tmpstr2);
			parse_p2p_Capability();
			break;				
		case TAG_DEVICE_ID 	:
			P2P_PRINT(" Device ID: %02x%02x%02x:%02x%02x%02x\n",
				pData[0],pData[1],pData[2],pData[3],pData[4],pData[5]);
			break;				
		case TAG_GROUP_OWNER_INTENT	: 
			P2P_PRINT(" Owner Intent: %d , Tie=%d\n",pData[0]>>1 , pData[0]&0x1);
			break;				
		case TAG_CONFIG_TIMEOUT :
			P2P_PRINT(" Config Timeout: = (%d,%d)\n",pData[0],pData[1]);
			break;				
		case TAG_LISTEN_CHANNEL  :
			P2P_PRINT(" Listen Channel: (country code),%d,%d \n",pData[3],pData[4]);
			break;				
		case TAG_P2P_GROUP_BSSID :
			P2P_PRINT(" p2p Group BSSID:%02x%02x%02x:%02x%02x%02x\n",
				pData[0],pData[1],pData[2],pData[3],pData[4],pData[5]);
			break;				
		case TAG_EXT_LISTEN_TIMING :	
			tmp_u16_1 = *(unsigned short*)pData;
			tmp_u16_2 = *(unsigned short*)(pData+2);			
			P2P_PRINT(" Extended Listen Timing: (%d ,%d)\n",tmp_u16_1,tmp_u16_2);
			break;				
		case TAG_INTEN_P2P_INTERFACE_ADDR :
			P2P_PRINT(" INTENED P2P INTERFACE ADDR=%02x%02x%02x:%02x%02x%02x\n",
				pData[0],pData[1],pData[2],pData[3],pData[4],pData[5]);
			break;				
		case TAG_P2P_MANAGEABILITY :	
			ShowValByBinary(pData[0] , tmpstr , 8);			
			P2P_PRINT(" P2P Manageability =%s\n",tmpstr);
			break;				
		case TAG_CHANNEL_LIST  :

			P2P_PRINT(" Channel List\n");
			parse_channel_list(pData , tag_len ,NULL);
			break;				
			
		case TAG_NOTICE_OF_ABSENCE :
			//TODO ; more detail
			P2P_PRINT(" Notice of Absence\n");
			break;				
		case TAG_P2P_DEVICE_INFO :

			P2P_PRINT(" P2P Device Info \n");
			parse_device_info(pData,tag_len,NULL);
			break;				
			
		case TAG_P2P_GROUP_INFO	 :

			P2P_PRINT(" p2p Group Info \n");
			parse_group_info(pData,tag_len);			
			break;				
			
		case TAG_P2P_GROUP_ID  :

			P2P_PRINT(" p2p Group ID\n");
			parse_group_id(pData,tag_len,NULL);
			break;				
			
		case TAG_P2P_INTERFACE	 :

			P2P_PRINT(" P2P INTERFACE \n");
			parse_p2p_interface(pData,tag_len);
			break;				
			
		case TAG_OPERATION_CHANNEL	: 
			strncpy(tmpstr , pData , 3);
			P2P_PRINT(" Operation Channel ,country code=%s operation class=%d ,channel number=%d  \n",
			tmpstr , pData[3],pData[4]);
			break;				
		case TAG_INVITATION_FLAGS	:
			ShowValByBinary(pData[0] , tmpstr , 8)	;		
			P2P_PRINT(" Invitation Flags=%s\n",tmpstr);
			break;				
		default:
			P2P_DEBUG("Unknow TAG ID \n");			
			break;	
	}

#endif
	*o_len = tag_len;
	return pData;
}


#define GET_INFO	0	// no meaning
int  p2p_get_role(struct rtl8192cd_priv *priv, 
	unsigned char *p2p_ie,int p2pIElen )
{
		unsigned char* p2p_capa;
		int tag_len = 0;		

		p2p_capa = p2p_search_tag(p2p_ie , p2pIElen , TAG_P2P_CAPABILITY , &tag_len);

		
		if(p2p_capa[1] & BIT(0))
			return R_P2P_GO; // GO
		else
			return R_P2P_DEVICE; // device 
		
}
void  p2p_get_device_info(struct rtl8192cd_priv *priv, 
	unsigned char *p2p_ie ,int p2pIElen ,struct device_info_s* dev_info_ptr)
{	
		unsigned char* p2p_sub_ie;
		int tag_len = 0;

		p2p_sub_ie = p2p_search_tag(p2p_ie , p2pIElen , TAG_P2P_DEVICE_INFO , &tag_len);
		if(p2p_sub_ie){
			parse_device_info(p2p_sub_ie,tag_len, dev_info_ptr);		
		}else{
			P2P_DEBUG("can't found DEVICE_INFO\n\n");
		}
			
}

int  p2p_get_GO_p2p_info(struct rtl8192cd_priv *priv, 
	unsigned char *p2p_ie ,int p2pIElen ,struct device_info_s* dev_info_ptr)
{	
		unsigned char* p2p_sub_ie=NULL;
		int tag_len = 0;

		/*ID3*/
		p2p_sub_ie = p2p_search_tag(p2p_ie , p2pIElen , TAG_DEVICE_ID , &tag_len);
		if(p2p_sub_ie ){
			memcpy(dev_info_ptr->dev_address , p2p_sub_ie , 6);
			//MAC_PRINT(dev_info_ptr->dev_address);
			return SUCCESS;			
		}else{
			return FAIL;
		}				
}

void  p2p_get_GO_wsc_info(struct rtl8192cd_priv *priv, 
	unsigned char *wsc_ie ,int wscIElen ,struct device_info_s *dev_info_ptr)
{
		unsigned char* p2p_sub_ie=NULL;
		int tag_len = 0;
		

		p2p_sub_ie = search_wsc_tag(wsc_ie, TAG_CONFIG_METHODS, wscIElen, &tag_len);		
		if(p2p_sub_ie){
			dev_info_ptr->config_method = *(unsigned short *)p2p_sub_ie ;
			//P2P_DEBUG("wsc method = %02x\n",dev_info_ptr->config_method);
		}

		p2p_sub_ie = search_wsc_tag(wsc_ie, TAG_DEVICE_NAME, wscIElen, &tag_len);		
		if(p2p_sub_ie){
			memcpy(dev_info_ptr->devname , p2p_sub_ie ,tag_len) ;
			dev_info_ptr->devname[tag_len]='\0';
			//P2P_DEBUG("device name = %s\n",dev_info_ptr->devicename);
		}
		

		
}



#define ADD_P2P_ATTRIBUTE	0	// no meaning


unsigned char *wsc_add_tlv(unsigned char *data, unsigned short id, int len, void *val)
{
	unsigned short size = htons(len);
	unsigned short tag = htons(id);

	memcpy(data, &tag, 2);
	memcpy(data+2, &size, 2);
	memcpy(data+4, val, len);

	return (data+4+len);
}



void add_primarydevtype(struct rtl8192cd_priv *priv  ,unsigned char * AttrPtr)
{


	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned short shortVal = 0;

	if(OPMODE & WIFI_AP_STATE)
		shortVal = P2P_device_category_id_AP;
	else
		shortVal = P2P_device_category_id_STA;
	
	memcpy(thisAttrPrt, &shortVal, 2);
	
	memcpy(thisAttrPrt+2, WSC_IE_OUI, 4);
	
	shortVal = P2P_device_sub_category_id;

	memcpy(thisAttrPrt+6, &shortVal, 2);	
		

}

unsigned char* add_Attr_status(struct rtl8192cd_priv *priv ,unsigned char * AttrPtr 
	,  unsigned char *p2pIELen ,unsigned char status )
{

	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned short attrlen = 1 ;	
	
	thisAttrPrt[0] = TAG_STATUE;
	attrlen = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen  ,2);	
	thisAttrPrt[3] = status;

	*p2pIELen += (1+3); 
	thisAttrPrt += (1+3);
	
	return thisAttrPrt;
}


unsigned char* add_Attr_minor_reason(struct rtl8192cd_priv *priv 
	,unsigned char * AttrPtr ,  unsigned char *p2pIELen ,unsigned char reason )
{

	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned short attrlen = 1 ;		
	thisAttrPrt[0] = TAG_MINOR_RES_CODE;
	attrlen = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen  ,2);	
	thisAttrPrt[3] = reason;
	*p2pIELen += (1+3); 
	thisAttrPrt += (1+3);	
	return thisAttrPrt;
}

unsigned char* add_Attr_capability(
	struct rtl8192cd_priv *priv ,unsigned char * AttrPtr ,
	unsigned char *p2pIELen ,struct p2p_device_peer *current_nego_peer)
{

	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned char deviceCap = 0;	
	unsigned char groupCap = 0;
	unsigned short attrlen = 2 ;	

	/* set devce cap*/
	if(P2PMODE == P2P_TMP_GO || P2PMODE == P2P_CLIENT)
	{
		deviceCap |= CLIENT_DISCOVERY;
	}



	/* set group cap*/
	/* i am prepare as GO*/
	if(current_nego_peer && current_nego_peer->role	== R_P2P_CLIENT){
		groupCap |= GCAP_IBSS_DIST ;
	}
	
	if(P2PMODE ==P2P_TMP_GO)
	{
		groupCap |= GCAP_GO;
		groupCap |= GCAP_IBSS_DIST ;
	}


	/*when i am pre-go , indicate that it's wps ongoing*/
	if(P2PMODE == P2P_PRE_GO ){
		groupCap |= GCAP_GO_FORMATION;		
	}
	
	if(priv->p2pPtr->persistent_go)
		groupCap |= GCAP_PRESISTENT_GO;

	if(priv->p2pPtr->p2p_go_limit)
		groupCap |= GCAP_GROUP_LIMIT;	



	thisAttrPrt[0] = TAG_P2P_CAPABILITY;
	attrlen = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen  ,2);
	thisAttrPrt[3] = deviceCap;
	thisAttrPrt[4] = groupCap;

	*p2pIELen += (2+3); 
	thisAttrPrt += (2+3);



	return thisAttrPrt;

}




/* SPEC 1.1 page 83*/ 
unsigned char* add_Attr_device_id(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{

	unsigned char* thisAttrPrt = AttrPtr;	
	unsigned short attrlen=6;
	
	thisAttrPrt[0] = TAG_DEVICE_ID;

	attrlen = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen  ,2);
	
	memcpy(thisAttrPrt+3 , GET_MY_HWADDR , 6);	

	
	*p2pIELen+= (6+3); 
	thisAttrPrt += (6+3);
	
	return thisAttrPrt;

}


/* SPEC 1.1 page 84*/ 
unsigned char* add_Attr_GO_intent(struct rtl8192cd_priv *priv ,unsigned char * AttrPtr 
	,  unsigned char *p2pIELen , int tieBreak)
{
	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned short attrlen = 1 ;	
	unsigned char intent = 0;

	thisAttrPrt[0] = TAG_GROUP_OWNER_INTENT;

	attrlen = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen  ,2);	

	intent	= priv->pmib->p2p_mib.p2p_intent<<1;

	if(tieBreak)
		intent |= 0x1;
	
	thisAttrPrt[3] = intent;

	*p2pIELen+= (1+3); 
	thisAttrPrt += (1+3);
	
	return thisAttrPrt;
}

/* SPEC 1.1 page 84*/ 
unsigned char* add_Attr_configration_timeout(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr ,
	unsigned char *p2pIELen , int MyRole)
{
	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned short attrlen = 2 ;	
	unsigned short attrlen2 = 2 ;		


	thisAttrPrt[0] = TAG_CONFIG_TIMEOUT;

	attrlen2 = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen2  ,2);	


	if(MyRole==R_P2P_GO){
		thisAttrPrt[3] = 0xff;
		thisAttrPrt[4] = 0;		
	}else{
		thisAttrPrt[3] = 0;			
		thisAttrPrt[4] = 0xff;
	}

	*p2pIELen+= (attrlen+3); 
	thisAttrPrt += (attrlen+3);
	
	return thisAttrPrt;

}



/* SPEC 1.1 page 85*/ 
unsigned char* add_Attr_listenChannel(
	struct rtl8192cd_priv *priv , unsigned char * AttrPtr , unsigned char *p2pIELen)
{

	unsigned char* thisAttrPrt = AttrPtr;	
	unsigned short attrlen=5;
	
	thisAttrPrt[0] = TAG_LISTEN_CHANNEL;	// ID6

	attrlen = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen  ,2);
	

	thisAttrPrt[3] = 'U';
	thisAttrPrt[4] = 'S';
	thisAttrPrt[5] = ' ';		
	thisAttrPrt[6] = 0x51;
	thisAttrPrt[7] = priv->pmib->p2p_mib.p2p_listen_channel;	
	
	*p2pIELen+= (5+3); 
	thisAttrPrt += (5+3);
	return thisAttrPrt;

}


/* SPEC 1.1 page 85;include in invitation req,no support now*/ 
unsigned char* add_Attr_group_bssid(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{

	unsigned char* thisAttrPrt = AttrPtr;		
	//unsigned short attrlen = 0 ;
	P2P_DEBUG("TODO\n");
	
	return thisAttrPrt;

}

/* SPEC 1.1 page 86*/ 
unsigned char* add_Attr_extended_listen_timing(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen,
	unsigned short period ,unsigned short interval )
{
	
	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned short attrlen = 4;	
	unsigned short attrlen2 ;
	
	thisAttrPrt[0] = TAG_EXT_LISTEN_TIMING; // ID8
	

	attrlen2 = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen2  ,2);	

	attrlen2 = cpu_to_le16(period);
	memcpy(thisAttrPrt+3 , (void *)&attrlen2  ,2);	

	attrlen2 = cpu_to_le16(interval);
	memcpy(thisAttrPrt+5 , (void *)&attrlen2  ,2);		

	*p2pIELen+= (attrlen+3); 
	thisAttrPrt += (attrlen+3);
	
	return thisAttrPrt;	

}


/* SPEC 1.1 page 87*/ 
unsigned char* add_Attr_intended_interface(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{
	unsigned char* thisAttrPrt = AttrPtr;	
	unsigned short attrlen=6;	
	thisAttrPrt[0] = TAG_INTEN_P2P_INTERFACE_ADDR;	// ID 9

	attrlen = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen  ,2);
	
	memcpy(thisAttrPrt+3 , GET_MY_HWADDR , 6);	

	
	*p2pIELen+= (6+3); 
	thisAttrPrt += (6+3);
	
	return thisAttrPrt;
	
}

/* SPEC 1.1 page 87*/ 
unsigned char* add_Attr_manageability(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{

/*	
	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned short attrlen = 0 ;
	P2P_DEBUG("TODO\n");
	
	return thisAttrPrt;
*/
	return NULL;
}


/* SPEC 1.1 page 88 ; ID 11 */ 
unsigned char* add_Attr_channel_list(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{

	unsigned char* thisAttrPrt = AttrPtr;	
	unsigned short attrlen=0;
	unsigned short attrlen2=0;
	int idx = 0;

	thisAttrPrt[0] = TAG_CHANNEL_LIST;	// ID 11
	
	attrlen2 = priv->p2pPtr->my_channel_list.Id11_len;
	attrlen= priv->p2pPtr->my_channel_list.Id11_len;
	//	P2P_DEBUG("channel list len=%d\n",priv->p2pPtr->my_channel_list.Id11_len);
	attrlen2 = cpu_to_le16(attrlen2);
	memcpy(&thisAttrPrt[1] , (void *)&attrlen2  ,2);	// attri len

	memcpy(&thisAttrPrt[3] , priv->p2pPtr->my_channel_list.country  , 3); //country code



	thisAttrPrt[6] = priv->p2pPtr->my_channel_list.reg_class[0].reg_class;
	thisAttrPrt[7] = priv->p2pPtr->my_channel_list.reg_class[0].channel_mun;
	for(idx=0;idx<11;idx++)
		thisAttrPrt[8+idx] =priv->p2pPtr->my_channel_list.reg_class[0].channel[idx];

	thisAttrPrt[19] = priv->p2pPtr->my_channel_list.reg_class[1].reg_class;
	thisAttrPrt[20] = priv->p2pPtr->my_channel_list.reg_class[1].channel_mun;
	for(idx=0;idx<4;idx++)
		thisAttrPrt[21+idx] =priv->p2pPtr->my_channel_list.reg_class[1].channel[idx];

	thisAttrPrt[25] = priv->p2pPtr->my_channel_list.reg_class[2].reg_class;
	thisAttrPrt[26] = priv->p2pPtr->my_channel_list.reg_class[2].channel_mun;
	for(idx=0;idx<5;idx++)
		thisAttrPrt[27+idx] =priv->p2pPtr->my_channel_list.reg_class[2].channel[idx];
			

	*p2pIELen+= (attrlen+3); 
	thisAttrPrt += (attrlen+3);
	return thisAttrPrt;
}


/* SPEC 1.1 page 89*/ 
unsigned char* add_Attr_notice_of_absence(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{
	unsigned char* thisAttrPrt = AttrPtr;	
	unsigned short attrlen = 2;	
	unsigned short attrlen2 = 0;
	
	thisAttrPrt[0] = TAG_NOTICE_OF_ABSENCE;	// ID 9

	attrlen2 = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen2  ,2);

	thisAttrPrt[3] = 0;	// index 
	thisAttrPrt[4] = 0;	// no oppPS , no CTWindow


	
	*p2pIELen+= (attrlen+3); 
	thisAttrPrt += (attrlen+3);
	
	return thisAttrPrt;
	
}


/* SPEC 1.1 page 85*/ 
unsigned char* add_Attr_device_info(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{

	struct wifi_mib *pmib=priv->pmib;	
	unsigned char* thisAttrPrt = AttrPtr;		
	unsigned short attrlen = 0 ;
	unsigned short lentmp = 0 ;	
	//unsigned short u16_tmp = 0 ;		

	thisAttrPrt[0] = TAG_P2P_DEVICE_INFO;

	/*fill attr len later*/
	thisAttrPrt+=3 ;  // id(1) + len(2)

	/*P2P device address*/
	memcpy(thisAttrPrt , pmib->dot11OperationEntry.hwaddr , 6); 
	thisAttrPrt+=6 ;	
	attrlen+=6 ;

	/*config method*/
	memcpy(thisAttrPrt , (void *)&pmib->p2p_mib.p2p_wsc_config_method , 2); 
	thisAttrPrt+=2 ;	
	attrlen+=2 ;

	add_primarydevtype(priv,thisAttrPrt);
	thisAttrPrt+=8 ;	
	attrlen+=8 ;	

	/* number of second dev type*/
	thisAttrPrt[0]=0;		
	thisAttrPrt+=1 ;	
	attrlen+=1 ;	


	/* add Device name ; WSC TLV (2+2+N)*/
	lentmp = strlen( pmib->p2p_mib.p2p_device_name );
	thisAttrPrt = wsc_add_tlv(thisAttrPrt, TAG_DEVICE_NAME,
		lentmp, pmib->p2p_mib.p2p_device_name);
	attrlen+= (lentmp + 4);	
			
		
	/*Attribute len */
	lentmp = cpu_to_le16(attrlen);	
	memcpy(AttrPtr+1 ,(void *)&lentmp ,2);

	*p2pIELen += (attrlen+3);	
	//p2p_debug_out("device info content", AttrPtr, attrlen);
	return thisAttrPrt;

}



/* SPEC 1.1 page 93*/ 
unsigned char* add_Attr_group_info(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{

	unsigned char* thisAttrPrt = AttrPtr;		

	unsigned char* tmpPtr = NULL;		
	int per_desc_len = 0;
	int idx ;
	unsigned short attrlen = 0 ;	
	unsigned short attrlen2 = 0 ;	
	int tmplen = 0 ;	

	P2P_DEBUG("\n");
	thisAttrPrt[0] = TAG_P2P_GROUP_INFO;

	thisAttrPrt +=3;
	
	for(idx=0 ; idx < MAX_P2P_CLIENT_MUN; idx++){
		if(priv->p2pPtr->assocPeers[idx].inuse){
			
			per_desc_len=0;
			tmpPtr = thisAttrPrt; // keep len addr
			thisAttrPrt +=1; // skip len
			
			memcpy(thisAttrPrt,priv->p2pPtr->assocPeers[idx].devInfo.dev_address,6);			
			thisAttrPrt +=6;
			per_desc_len +=6;

			memcpy(thisAttrPrt,priv->p2pPtr->assocPeers[idx].if_addr,6);
			thisAttrPrt +=6;
			per_desc_len +=6;			

			thisAttrPrt[0]=priv->p2pPtr->assocPeers[idx].dev_cap ; // len ==1
			thisAttrPrt +=1;
			per_desc_len +=1;

			memcpy(thisAttrPrt,&priv->p2pPtr->assocPeers[idx].devInfo.config_method,2);
			thisAttrPrt +=2;
			per_desc_len +=2;
			
			memcpy(thisAttrPrt,priv->p2pPtr->assocPeers[idx].devInfo.pri_dev_type,8);
			thisAttrPrt +=8;
			per_desc_len +=8;
			
			thisAttrPrt[0] = 0 ; // second dev type count == 0
			thisAttrPrt +=1;
			per_desc_len +=1;


			/*device name ; WSC TLV (2+2+N)*/
			tmplen = strlen(priv->p2pPtr->assocPeers[idx].devInfo.devname);			
			thisAttrPrt = wsc_add_tlv(thisAttrPrt, TAG_DEVICE_NAME,
				tmplen, priv->p2pPtr->assocPeers[idx].devInfo.devname);
			
			per_desc_len += (2+2+tmplen);
	

			tmpPtr[0] = per_desc_len;
			attrlen += per_desc_len ;
			
		}
	}
		


	attrlen2 = attrlen;
	attrlen2 = cpu_to_le16(attrlen2);
	memcpy(&AttrPtr[1] , (void *)&attrlen2  ,2);	


	*p2pIELen += (attrlen+3); 
	thisAttrPrt += (attrlen+3);

	return thisAttrPrt;	

}

/* SPEC 1.1 page 94*/ 
unsigned char* add_Attr_group_id(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{

	unsigned char* thisAttrPrt = AttrPtr;		

	unsigned short attrlen = 0 ;	
	unsigned short attrlen2 = 0 ;	
	
	thisAttrPrt[0] = TAG_P2P_GROUP_ID;


	memcpy(&thisAttrPrt[3] , GET_MY_HWADDR , 6);	
	attrlen+=6;
	
	memcpy(&thisAttrPrt[9] , priv->p2pPtr->my_GO_ssid, priv->p2pPtr->my_GO_ssid_len);	
	attrlen += priv->p2pPtr->my_GO_ssid_len;

	attrlen2 = attrlen;
	attrlen2 = cpu_to_le16(attrlen2);
	memcpy(&thisAttrPrt[1] , (void *)&attrlen2  ,2);	


	*p2pIELen += (attrlen+3); 
	thisAttrPrt += (attrlen+3);

	return thisAttrPrt;	

}



/* SPEC 1.1 page 94*/ 
unsigned char* add_Attr_Target_group_id(	struct rtl8192cd_priv *priv , 
	unsigned char* AttrPtr , unsigned char *p2pIELen ,
	unsigned char* tarssid , unsigned char* tardevaddr)
{

	unsigned char* thisAttrPrt = AttrPtr;		

	unsigned short attrlen = 0 ;	
	unsigned short attrlen2 = 0 ;	
	
	thisAttrPrt[0] = TAG_P2P_GROUP_ID;


	memcpy(&thisAttrPrt[3] , tardevaddr, 6);	
	attrlen+=6;
	
	memcpy(&thisAttrPrt[9] , tarssid, strlen(tarssid));	
	attrlen += strlen(tarssid) ;

	attrlen2 = attrlen;
	attrlen2 = cpu_to_le16(attrlen2);
	memcpy(&thisAttrPrt[1] , (void *)&attrlen2  ,2);	


	*p2pIELen += (attrlen+3); 
	thisAttrPrt += (attrlen+3);

	return thisAttrPrt;	

}


/* SPEC 1.1 page 95*/ 
unsigned char* add_Attr_operation_channel(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{

	unsigned char* thisAttrPrt = AttrPtr;	
	unsigned short attrlen=5;
	
	thisAttrPrt[0] = TAG_OPERATION_CHANNEL;

	attrlen = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen  ,2);
	

	thisAttrPrt[3] = 'U';
	thisAttrPrt[4] = 'S';
	thisAttrPrt[5] = ' ';
	
	if(priv->pmib->p2p_mib.p2p_op_channel>=1 && priv->pmib->p2p_mib.p2p_op_channel<=14)
		thisAttrPrt[6] = 81;
	else 	if(priv->pmib->p2p_mib.p2p_op_channel>=36 && priv->pmib->p2p_mib.p2p_op_channel<=48)
		thisAttrPrt[6] = 115;
	else 	if(priv->pmib->p2p_mib.p2p_op_channel>=149 && priv->pmib->p2p_mib.p2p_op_channel<=165)
		thisAttrPrt[6] = 124;

	thisAttrPrt[7] = priv->pmib->p2p_mib.p2p_op_channel;	
	
	*p2pIELen+= (5+3); 
	thisAttrPrt += (5+3);
	
	return thisAttrPrt;

}

/* SPEC 1.1 page 96*/ 
unsigned char* add_Attr_invitation_flag(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{
	P2P_DEBUG("TODO\n");
	return NULL;
}


/* SPEC 1.1 page 94*/ 
unsigned char* add_Attr_interface(
	struct rtl8192cd_priv *priv , unsigned char* AttrPtr , unsigned char *p2pIELen)
{


	unsigned char* thisAttrPrt = AttrPtr;	
	unsigned short attrlen=0;
	unsigned short attrlen2=0;	
	thisAttrPrt[0] = TAG_P2P_INTERFACE;

	attrlen2 = cpu_to_le16(attrlen);
	memcpy(thisAttrPrt+1 , (void *)&attrlen2  ,2);
	
	
	*p2pIELen+= (attrlen+3); 
	thisAttrPrt += (attrlen+3);
	
	return thisAttrPrt;

}	


#define BUILD_IE 0	// no meaning


int wsc_build_probe_rsp_ie(struct rtl8192cd_priv *priv ,
	unsigned char *data , unsigned short DEVICE_PASSWORD_ID)	
{


	
	unsigned char *pMsg;
	unsigned char byteVal;
	unsigned short shortVal;
	unsigned char tmpbuf2[100];
	int len5;

	/*Element ID*/ 
	data[0] = WSC_IE_ID;

	/*OUI*/ 	
	memcpy(&data[2], WSC_IE_OUI, 4);
	pMsg = &data[2+4];

	/*Version*/ 
	byteVal = WSC_VER;
	pMsg = wsc_add_tlv(pMsg, TAG_VERSION, 1, (void *)&byteVal);


	/*config state*/
	//fixed to AP mode
	byteVal = 2; //configured	
	
	pMsg = wsc_add_tlv(pMsg, TAG_SIMPLE_CONFIG_STATE, 1, (void *)&byteVal);
				
	// fixed to AP mode 
	byteVal = RSP_TYPE_AP;	
	pMsg = wsc_add_tlv(pMsg, TAG_RESPONSE_TYPE2, 1, (void *)&byteVal);

	/*UUID-E*/
	//pMsg = wsc_add_tlv(pMsg, TAG_UUID_E, UUID_LEN, (void *)priv->p2pPtr->uuid);	
	//pMsg = wsc_add_tlv(pMsg, TAG_MANUFACTURER, strlen(priv->p2pPtr->manufacturer), priv->p2pPtr->manufacturer);
	//pMsg = wsc_add_tlv(pMsg, TAG_MODEL_NAME, strlen(priv->p2pPtr->model_name), priv->p2pPtr->model_name);	
	//pMsg = wsc_add_tlv(pMsg, TAG_MODEL_NUMBER, strlen(priv->p2pPtr->model_num), priv->p2pPtr->model_num);
	//pMsg = wsc_add_tlv(pMsg, TAG_SERIAL_NUM, strlen(priv->p2pPtr->serial_num), priv->p2pPtr->serial_num);


	/*----TAG_PRIMARY_DEVICE_TYPE -----start*/
	
	if(OPMODE & WIFI_AP_STATE)
		shortVal = P2P_device_category_id_AP;
	else
		shortVal = P2P_device_category_id_STA;
	
	memcpy(tmpbuf2, &shortVal, 2);

	memcpy(&tmpbuf2[2], WSC_IE_OUI, 4);
	
	shortVal = P2P_device_sub_category_id;
	memcpy(&tmpbuf2[6], &shortVal, 2);		
	pMsg = wsc_add_tlv(pMsg, TAG_PRIMARY_DEVICE_TYPE, 8, (void *)tmpbuf2);

	/*----TAG_PRIMARY_DEVICE_TYPE -----end*/

	/*device name */
	pMsg = wsc_add_tlv(pMsg, TAG_DEVICE_NAME,
		strlen(priv->pmib->p2p_mib.p2p_device_name), priv->pmib->p2p_mib.p2p_device_name);

	/*config method*/
	pMsg = wsc_add_tlv(pMsg, TAG_CONFIG_METHODS, 2, (void *)&priv->pmib->p2p_mib.p2p_wsc_config_method);

	shortVal = DEVICE_PASSWORD_ID;
	pMsg = wsc_add_tlv(pMsg, TAG_DEVICE_PASSWORD_ID, 2, &shortVal);

	/*Element Length*/ 
	len5 = (int)(((unsigned long)pMsg)-((unsigned long)data)) -2;
	data[1] = (unsigned char)len5;

	return (len5+2);
}



// (TLV , T(1),L(1))
int p2p_build_probe_rsp_ie(struct rtl8192cd_priv *priv, unsigned char *data)
{
	unsigned char p2pIELen = 0;
	unsigned char* thisAttrPrt = data;		


	thisAttrPrt[0]=_P2P_IE_;

	thisAttrPrt+=2;
	
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen+=4;
	thisAttrPrt+=4;


	/*ID2*/
	thisAttrPrt = add_Attr_capability( priv, thisAttrPrt, &p2pIELen ,NULL);

	/*extended listen timing (OPTIONAL)*/

	/*notice of absence ;shell only tx by GO*/


	/*ID13*/
	thisAttrPrt = add_Attr_device_info(priv, thisAttrPrt ,&p2pIELen);

	/*ID14 ,only include by GO*/
	if(P2PMODE == P2P_TMP_GO){
		thisAttrPrt = add_Attr_group_info(priv, thisAttrPrt ,&p2pIELen);	
	}


	data[1]=p2pIELen;

	//p2p_debug_out("probe_rsp", data, p2pIELen+2);	

	return p2pIELen+2;
	
}


int p2p_build_probe_req_ie(struct rtl8192cd_priv *priv, unsigned char *data)
{

	unsigned char p2pIELen=0;
	unsigned char* thisAttrPrt = data;		
	
	thisAttrPrt[0]=_P2P_IE_;

	thisAttrPrt+=2;
	
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen += 4;
	thisAttrPrt += 4;


	/*ID2*/
	thisAttrPrt = add_Attr_capability( priv, thisAttrPrt, &p2pIELen ,NULL);

	/*ID3 ; for search spec address */
	if(!is_zero_ether_addr(priv->p2pPtr->spec_find_dev_addr))
		thisAttrPrt = add_Attr_device_info(priv, thisAttrPrt ,&p2pIELen);


	/*ID6*/
	thisAttrPrt = add_Attr_listenChannel( priv, thisAttrPrt, &p2pIELen);

	
	/*extended listen timing ;(OPTIONAL) no support now*/	

	
	/*ID17 ; only include by GO*/
	if(P2PMODE ==P2P_TMP_GO){
		thisAttrPrt = add_Attr_operation_channel( priv, thisAttrPrt, &p2pIELen);
	}
		

	data[1]= p2pIELen;
	
	//p2p_debug_out("probe_req", data, p2pIELen+2);
	
	return p2pIELen+2;
	
}





int p2p_build_beacon_ie(struct rtl8192cd_priv *priv, unsigned char *data)
{

	unsigned char p2pIELen = 0;
	unsigned char* thisAttrPrt = data;		

	P2P_DEBUG("\n");
	thisAttrPrt[0]=_P2P_IE_;

	thisAttrPrt+=2;
	
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen+=4;
	thisAttrPrt+=4;


	/*ID2*/	
	thisAttrPrt = add_Attr_capability( priv, thisAttrPrt, &p2pIELen ,NULL);
	
	/*ID3*/
	thisAttrPrt = add_Attr_device_id(priv, thisAttrPrt ,&p2pIELen);

	data[1]=p2pIELen;

	//p2p_debug_out("beacon p2p IE", data, p2pIELen+2);	

	return p2pIELen+2;
	
}

int p2p_build_assocReq_ie(struct rtl8192cd_priv *priv, unsigned char *data)
{

	unsigned char p2pIELen = 0;
	unsigned char* thisAttrPrt = data;		

	P2P_DEBUG("\n");
	thisAttrPrt[0]=_P2P_IE_;

	thisAttrPrt+=2;
	
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen+=4;
	thisAttrPrt+=4;


	/* ID2*/
	thisAttrPrt = add_Attr_capability( priv, thisAttrPrt, &p2pIELen ,NULL);
	
	/* ID13 */
	thisAttrPrt = add_Attr_device_info(priv, thisAttrPrt ,&p2pIELen);

	data[1]=p2pIELen;

	p2p_debug_out("assoc p2p IE", data, p2pIELen+2);	

	return p2pIELen+2;
	
}


int p2p_build_assocRsp_ie(struct rtl8192cd_priv *priv, unsigned char *data
		,unsigned char status)
{

	unsigned char p2pIELen = 0;
	unsigned char* thisAttrPrt = data;		

	P2P_DEBUG("\n");
	thisAttrPrt[0]=_P2P_IE_;
	thisAttrPrt+=2;
	
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen+=4;
	thisAttrPrt+=4;

	/*ID0*/
	thisAttrPrt = add_Attr_status(priv ,  thisAttrPrt, &p2pIELen ,status);

	data[1]=p2pIELen;

	p2p_debug_out("AssocRsp p2p IE", data, p2pIELen+2);	

	return p2pIELen+2;
	
}


int p2p_build_deassoc_ie(struct rtl8192cd_priv *priv, 
	unsigned char *data ,unsigned char reason)
{
	unsigned char p2pIELen = 0;
	unsigned char* thisAttrPrt = data;		
	P2P_DEBUG("\n");
	thisAttrPrt[0]=_P2P_IE_;	
	thisAttrPrt+=2;		
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen+=4;
	thisAttrPrt+=4;

	/*ID1*/
	thisAttrPrt = add_Attr_minor_reason( priv, thisAttrPrt, &p2pIELen ,reason);
	
	data[1]=p2pIELen;
	//p2p_debug_out("disassoc p2p IE", data, p2pIELen+2);	
	return p2pIELen+2;
	
}
	

#define HANDLE_P2P_ACTION_FRAME	0	// no meaning

#define GO_mdoe_handle_P2P_action_frame	0	// no meaning


int p2p_issue_GO_discovery_req(
	struct rtl8192cd_priv *priv ,
	unsigned char *da)
{

	unsigned char	*pbuf = NULL;
	DECLARE_TXINSN(txinsn);

	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = _6M_RATE_;
	txinsn.lowest_tx_rate = txinsn.tx_rate;
	txinsn.fixed_rate = 1;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_GO_dis_req_fail1;

	txinsn.phdr = get_wlanhdr_from_poll(priv);

	if (txinsn.phdr == NULL)
		goto issue_GO_dis_req_fail1;


	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));


	pbuf[0] = _VENDOR_ACTION_ID_;
	memcpy(&pbuf[1],WFA_OUI_PLUS_TYPE,4);
	pbuf[5] = P2P_GO_DISCOVERY;	

	txinsn.fr_len += 6;

	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), da, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	


	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS){
		
		P2P_PRINT("\n");
		P2P_DEBUG("TX GO discovery Req success!!\n");
		MAC_PRINT(da);

		/*only expect will rx ack, that is all*/
		return SUCCESS;
	}

issue_GO_dis_req_fail1:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	
	return FAIL;
	

}




/* page 68 */ 
int p2p_issue_presence_rsp(struct rtl8192cd_priv *priv , unsigned char *da)
{

	unsigned char	*pbuf;
	unsigned char	*thisAttrPrt;
	unsigned char	 p2pIELen = 0;	
	unsigned char	*P2PIEstart=NULL;	
	
	DECLARE_TXINSN(txinsn);

	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = _6M_RATE_;
	txinsn.lowest_tx_rate = txinsn.tx_rate;
	txinsn.fixed_rate = 1;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_presenceRsp_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);

	if (txinsn.phdr == NULL)
		goto issue_presenceRsp_fail;


	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));


	pbuf[0] = _VENDOR_ACTION_ID_;
	memcpy(&pbuf[1],WFA_OUI_PLUS_TYPE,4);
	pbuf[5] = P2P_PRESENCE_RSP;	
	pbuf[6] = priv->p2pPtr->presence_rx_dialog_token;	


	
	/*P2P ID*/ 
	thisAttrPrt = pbuf + _P2P_ACTION_IE_OFFSET_;
	p2pIELen = 0;
	P2PIEstart = thisAttrPrt;
	
	thisAttrPrt[0] = P2P_IE_ID;
	thisAttrPrt+=2;  // ID + LEN

	/*P2P OUI+TYPE*/
	memcpy(thisAttrPrt, WFA_OUI_PLUS_TYPE, 4);
	thisAttrPrt+=4; // OUI LEN
	p2pIELen +=4;

	/*ID0*/
	thisAttrPrt = add_Attr_status(priv ,  thisAttrPrt, &p2pIELen ,P2P_SC_SUCCESS);
	

	/*ID12*/
	thisAttrPrt = add_Attr_notice_of_absence(priv ,  thisAttrPrt, &p2pIELen );

	P2PIEstart[1] = p2pIELen;

	txinsn.fr_len += ((int)(((unsigned long)thisAttrPrt)-((unsigned long)pbuf)));

	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), da, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	


	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS){

		P2P_PRINT("\n");		
		P2P_DEBUG("TX Presence Rsp OK\n");
		MAC_PRINT(da);
		return SUCCESS;
	}

issue_presenceRsp_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	return FAIL;

	
}


/* page 68 */ 
void p2p_on_presence_req(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo)
{
	/*start of action frame content*/
	unsigned char *pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	

	int tmplen=0;			
	unsigned char* tmpPtr=NULL;		

	int foundP2PIE = 0;	
	unsigned char *P2PIEPtr = NULL ;	
	int P2PIElen=0;	
	unsigned char *p2p_sub_ie=NULL;
	
	priv->p2pPtr->presence_rx_dialog_token = pframe[6] ;

	//==========================================================================
	/*get P2P IE*/ 
	tmpPtr = pframe + _P2P_ACTION_IE_OFFSET_;
	tmplen = 0;
	
	for(;;)
	{
		tmpPtr = get_ie(tmpPtr, _P2P_IE_, &tmplen,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_ACTION_IE_OFFSET_ - tmplen);		
	
		if(tmpPtr!=NULL){
			if (!memcmp(tmpPtr+2, WFA_OUI_PLUS_TYPE, 4)) {
				P2PIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				P2PIEPtr = tmpPtr+6 ;
				foundP2PIE = 1;				
				break;
			}
		}else{
			break ;
		}
		tmpPtr = tmpPtr + tmplen +2 ;		
	}

	if(foundP2PIE){
		p2p_sub_ie = p2p_search_tag(P2PIEPtr, P2PIElen, TAG_NOTICE_OF_ABSENCE, &tmplen);
		if (p2p_sub_ie) {
			//parse_p2p_NOA(priv, p2p_sub_ie, tmplen, GetSequence(pframe));
			// we don't support PS under GO now , do nothing 
		}
		else {
			P2P_DEBUG("can't found NOA IE\n");
		}

	}else{
		P2P_DEBUG("can't found P2P IE\n");	
	}	
	//=========================================================================
	//==========================================================================
	p2p_issue_presence_rsp(priv,pfrinfo->sa);
	
	
}


// page 109
int	P2P_on_action(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{

	unsigned char *pframe=NULL;
	pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	//start of action frame content
	
	switch(pframe[5]){

		case P2P_NOA:
			P2P_PRINT("\n");
			P2P_DEBUG("RX P2P NOA\n");
			if(P2PMODE == P2P_CLIENT){
				
			}else{
				P2P_DEBUG("but no under client mode\n");
			}
			break;
		case P2P_PRESENCE_REQ:
			P2P_PRINT("\n");			
			P2P_DEBUG("RX P2P Presence Req\n");				
			if(P2PMODE == P2P_TMP_GO){
				p2p_on_presence_req(priv,pfrinfo);
			}else{
				P2P_DEBUG("but no under GO mode\n");
			}			
			break;
		case P2P_PRESENCE_RSP:
			P2P_PRINT("\n");
			P2P_DEBUG("RX P2P Presence Rsp\n");	
			if(P2PMODE == P2P_CLIENT){
				
			}else{
				P2P_DEBUG("but no under client mode\n");
			}			
			break;

		case P2P_GO_DISCOVERY:
			P2P_PRINT("\n");			
			P2P_DEBUG("RX P2P GO DISCOVERY,just ack\n");			
			break;		
		default:
			P2P_DEBUG("!!! Unknow type p2p action \n");			
			break;			
	}
	return 0;
}


#define HANDLE_P2P_PUBLIC_ACTION_FRAME	0	// no meaning


int p2p_issue_device_discovery_rsp(
	struct rtl8192cd_priv *priv ,
	unsigned char *da , unsigned char status)
{

	unsigned char	*pbuf = NULL;
	unsigned char	*IEStart=NULL;
	unsigned char	*thisAttrPrt=NULL;	
	unsigned char	 p2pIELen = 0;		
	DECLARE_TXINSN(txinsn);
	P2P_DEBUG("\n");		

	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = _6M_RATE_;
	txinsn.lowest_tx_rate = txinsn.tx_rate;
	txinsn.fixed_rate = 1;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_dev_dis_rsp_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);

	if (txinsn.phdr == NULL)
		goto issue_dev_dis_rsp_fail;


	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));


	pbuf[0] = CATEGORY_P2P_PUBLIC_ACTION;
	pbuf[1] = ACTIONY_P2P_PUBLIC_ACTION;
	memcpy(&pbuf[2],WFA_OUI_PLUS_TYPE,4);
	pbuf[6] = P2P_DEV_DISC_RESP;
	pbuf[7] = priv->p2pPtr->dev_dis_req_dialog_token ;	

	thisAttrPrt = pbuf + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	IEStart = pbuf + _P2P_PUBLIC_ACTION_IE_OFFSET_;	//anchor

	thisAttrPrt = add_Attr_status(priv , thisAttrPrt, &p2pIELen ,status);
	IEStart[1] = p2pIELen;

	txinsn.fr_len += ((int)(((unsigned long)thisAttrPrt)-((unsigned long)pbuf)));

	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), da, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	


	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS){		
		P2P_PRINT("\n");
		P2P_DEBUG("TX Device Discovery Rsp success!!\n");
		MAC_PRINT(da);
		return SUCCESS;
	}

issue_dev_dis_rsp_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	
	return FAIL;
	

}


void p2p_on_device_discovery_req(
	struct rtl8192cd_priv *priv ,
	struct rx_frinfo *pfrinfo)
{
	/*start of action frame content*/
	unsigned char *pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	
	unsigned char *IEptr = NULL;
	int	tmplen = 0;
	int foundP2PIE=0;
	int p2pIElen = 0 ;
	unsigned char *p2pIEPtr = NULL;	
	int idx;
	struct p2p_device_peer *current_nego_peer=NULL; 	
	
	priv->p2pPtr->dev_dis_req_dialog_token = pframe[7];

	/*-----------------------------get P2P IE(start)------------------------------*/ 
	IEptr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	
	for(;;)
	{
		IEptr = get_ie(IEptr, _P2P_IE_, &tmplen,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
	
		if(IEptr){
			if (!memcmp(IEptr+2, WFA_OUI_PLUS_TYPE, 4)) {
				p2pIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				p2pIEPtr = IEptr+6 ;
				foundP2PIE = 1;				
				break;
			}
		}else{
			break ;
		}
		IEptr = IEptr + tmplen +2 ;		
	}
	
	if(foundP2PIE){		

		/*only GO need process P2P IE in provision req*/
		current_nego_peer = &priv->p2pPtr->ongoing_nego_peer ;
		memset((void *)current_nego_peer , 0 ,sizeof(struct p2p_device_peer));

		// ID15
		IEptr = p2p_search_tag(p2pIEPtr, p2pIElen,  TAG_P2P_GROUP_ID , &tmplen);
		if(IEptr){
			parse_group_id(IEptr , tmplen ,current_nego_peer);
		}	

		if(memcmp(current_nego_peer->group_bssid , GET_MY_HWADDR ,6)){
			P2P_DEBUG("bssid no match!!\n");
			return;
		}
			
		// ID3
		IEptr = p2p_search_tag(p2pIEPtr, p2pIElen,  TAG_DEVICE_ID , &tmplen);
		if(IEptr){
			for(idx=0;idx<MAX_P2P_CLIENT_MUN ; idx++){
				if(!memcmp(priv->p2pPtr->assocPeers[idx].devInfo.dev_address,IEptr,6)){
					// some p2p device (no need assoc) search client that assoc with me
					if(priv->p2pPtr->assocPeers[idx].dev_cap & CLIENT_DISCOVERY){

						p2p_issue_GO_discovery_req(priv,IEptr);
						// we assume client will ack me
						p2p_issue_device_discovery_rsp(priv,pfrinfo->sa,P2P_SC_SUCCESS);						
						return ;
					}
				}
			}		
		}
				
	}
	/*-----------------------------get P2P IE(end)------------------------------*/ 
	p2p_issue_device_discovery_rsp(priv,pfrinfo->sa,P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE);	
	return ;

}



void p2p_on_device_discovery_rsp(
	struct rtl8192cd_priv *priv ,
	struct rx_frinfo *pfrinfo)
{
	// TODO ?	
	return;
}

int	P2P_on_public_action(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	/*start of action frame content*/
	unsigned char *pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	
	
	if(memcmp(pfrinfo->da , GET_MY_HWADDR , 6)){
		return 0;		
	}
	
	if(pframe[6] ==P2P_GO_NEG_REQ || pframe[6] ==P2P_GO_NEG_RESP || pframe[6] ==P2P_GO_NEG_CONF){
		if(P2PMODE != P2P_DEVICE){
			P2P_DEBUG("GO / client don't process GO nego frames\n");
			MAC_PRINT(pfrinfo->sa);
			return 0;
		}
	}
	if(pframe[6] == P2P_PROV_DISC_REQ || pframe[6] == P2P_PROV_DISC_RSP){
		if(P2PMODE == P2P_CLIENT){
			P2P_DEBUG("client don't process provision req\n");
			return 0;
		}
	}


	
	switch(pframe[6]){

		case P2P_GO_NEG_REQ:
			P2P_PRINT("\n\n");
			P2P_DEBUG("RX GO Nogotiation Req\n");				
			p2p_on_GO_nego_req(priv,pfrinfo);			
			break;
		case P2P_GO_NEG_RESP:
			P2P_PRINT("\n\n");			
			P2P_DEBUG("RX GO Nogotiation Rsp\n");				
			p2p_on_GO_nego_rsp(priv,pfrinfo);			
			break;
		case P2P_GO_NEG_CONF:
			P2P_PRINT("\n\n");			
			P2P_DEBUG("RX GO Nogotiation Confirmation\n");
			p2p_on_GO_nego_conf(priv,pfrinfo);
			break;

		case P2P_INVITATION_REQ:
			P2P_DEBUG("P2P invitation Req\n");
			
			break;
		case P2P_INVITATION_RESP:
			P2P_DEBUG("P2P invitation Rsp\n");
			
			break;

		case P2P_DEV_DISC_REQ:
			P2P_PRINT("\n\n");			
			P2P_DEBUG("Rx Device Discoverability Req\n");

			if(P2PMODE == P2P_TMP_GO)		// process only under GO mode
				p2p_on_device_discovery_req(priv,pfrinfo);
			
			break;

		case P2P_DEV_DISC_RESP:
			
			P2P_DEBUG("Device Discoverability Rsp\n");
			if(P2PMODE == P2P_CLIENT)		// process only under client mode
				p2p_on_device_discovery_rsp(priv,pfrinfo);
			
			break;

		case P2P_PROV_DISC_REQ:
			P2P_PRINT("\n\n");
			P2P_DEBUG("RX  Provision-Discovery 	Req\n");
			p2p_on_provision_req(priv,pfrinfo);
			break;
		case P2P_PROV_DISC_RSP:
			P2P_PRINT("\n\n");
			P2P_DEBUG("Rx  Provision-Discovery 	Rsp\n");
			p2p_on_provision_rsp(priv,pfrinfo);
			
			break;			
		default:
			P2P_DEBUG("!!! Unknow type p2p public action \n");			
			break;			
	}
	
	return 0;
	
}


void P2P_1sec_timer(struct rtl8192cd_priv *priv)
{
	//static int p2p_1sec_count = 0;
	
	if(P2PMODE == P2P_DEVICE){	
		
		if(priv->p2pPtr->wait2listenState){
			priv->p2pPtr->wait2listenState -- ;
			P2P_DEBUG("befor listen(%d)\n",priv->p2pPtr->wait2listenState);
			if(priv->p2pPtr->wait2listenState == 0){
				P2P_DEBUG("enter listen\n");
				P2P_listen(priv,NULL);
			}
		}	
		

		
		/*handle active send provision discovery req*/
		if(priv->p2pPtr->provision_req_timeout){
			priv->p2pPtr->provision_req_timeout--;
			
			if ((P2P_STATE == P2P_S_PROVI_WAIT_RSP) 
				&& priv->p2pPtr->provision_req_timeout
				&& (priv->p2pPtr->provision_req_timeout%2 == 0))
			{
				p2p_issue_provision_req(priv);		

			}else 
			if((P2P_STATE == P2P_S_PROVI_WAIT_RSP) 
				&& priv->p2pPtr->provision_req_timeout==0){

				/*timeout ; back to listen mode*/
				P2P_DEBUG("->\n");				
				P2P_listen(priv,NULL);
			}

		}

		/*handle active send provision discovery req*/	
		if(priv->p2pPtr->go_nego_on_going_timeout>0){
			priv->p2pPtr->go_nego_on_going_timeout --;
			
			if(priv->p2pPtr->go_nego_on_going_timeout==0){
				priv->p2pPtr->go_nego_on_going = 0;
				P2P_DEBUG("->\n");				
				P2P_listen(priv,NULL);
			}

			if(P2P_STATE == P2P_S_NEGO_WAIT_RSP && priv->p2pPtr->go_nego_on_going){
				if(priv->p2pPtr->go_nego_on_going_timeout%2 == 0){
					p2p_issue_GO_nego_req(priv);
				}
			}		

		}

		/*handle passive rx nego req flow*/	
		if(P2P_STATE == P2P_S_NEGO_WAIT_CONF ){
			if(priv->p2pPtr->wait_nego_conf_timeout > 0 ){
				priv->p2pPtr->wait_nego_conf_timeout -- ;
				if(priv->p2pPtr->wait_nego_conf_timeout == 0){

					if(priv->p2pPtr->go_nego_on_going){
						P2P_DEBUG("unlock go_nego_ongoing\n\n");
						priv->p2pPtr->go_nego_on_going = 0;
					}
					P2P_DEBUG("->\n");
					P2P_listen(priv,NULL);
				}
			}
		}


	}
	else if(P2PMODE == P2P_PRE_CLIENT){
		if(priv->p2pPtr->pre_client_timeout >= 0 ){			

			priv->p2pPtr->pre_client_timeout -=1;

			if(priv->p2pPtr->pre_client_timeout == 0){
				P2P_DEBUG(" WPS process exceed %d seconds,back to device mode\n",WSC_MODE_WAIT_TIME);
				p2pcmd_backtoDev(priv,NULL);
			}
		}		
	}
	else if(P2PMODE == P2P_PRE_GO){
		if(priv->p2pPtr->pre_go_timeout >= 0 ){			
			priv->p2pPtr->pre_go_timeout -= 1;			
			if(priv->p2pPtr->pre_go_timeout == 0){
				P2P_DEBUG(" WPS process exceed %d seconds,back to device mode\n",WSC_MODE_WAIT_TIME);
				p2pcmd_backtoDev(priv,NULL);
			}
		}		
	}
	/*
	else if(P2PMODE == P2P_TMP_GO){
		p2p_1sec_count++;

		if(p2p_1sec_count%10 == 0)	// 10 seconds check once
			P2P_chk_assoc_client(priv);
	}
	*/

	
	return ;
}



#define Active_TX_provision_req	0	// no meaning

/*P2P discovery related functions*/
int req_p2p_provision_req(struct rtl8192cd_priv *priv, unsigned char *data)
{

	struct provision_comm *provision_comm_ptr = (struct provision_comm *)data;
	int idx;
	int found;

	P2P_DEBUG("trigger from UI ; wsc method=%02x \n",
		provision_comm_ptr->wsc_config_method);
	
	memcpy(priv->p2pPtr->target_device_addr,provision_comm_ptr->dev_address,6);
	priv->p2pPtr->target_device_channel = provision_comm_ptr->channel;
	P2P_DEBUG("\n\ntarget dev on channel(%d)\n\n",priv->p2pPtr->target_device_channel);

	/*config method ; reference page38*/
	if(provision_comm_ptr->wsc_config_method == CONFIG_METHOD_PBC){
		
		priv->p2pPtr->wsc_method_to_target_dev =	CONFIG_METHOD_PBC ;
		priv->p2pPtr->dev_passwd_to_tar_dev = PASS_ID_PB;		/*for nego req use*/
		
	}else if(provision_comm_ptr->wsc_config_method == CONFIG_METHOD_PIN
		||  provision_comm_ptr->wsc_config_method == CONFIG_METHOD_DISPLAY){
		
		priv->p2pPtr->wsc_method_to_target_dev =	CONFIG_METHOD_KEYPAD ;
		priv->p2pPtr->dev_passwd_to_tar_dev = PASS_ID_REG;	/*for nego req use*/	
		
	}else if(provision_comm_ptr->wsc_config_method == CONFIG_METHOD_KEYPAD){
	
		priv->p2pPtr->wsc_method_to_target_dev =	CONFIG_METHOD_DISPLAY ;
		priv->p2pPtr->dev_passwd_to_tar_dev = PASS_ID_USER; /*for nego req use*/ 
		
	}
	

	found = 0;
	for(idx=0;idx < priv->site_survey.count_backup; idx++){
		if(!memcmp(priv->site_survey.bss_backup[idx].p2paddress ,priv->p2pPtr->target_device_addr ,6))
		{
			priv->p2pPtr->target_device_role = priv->site_survey.bss_backup[idx].p2prole ;
			strcpy(priv->p2pPtr->target_device_ssid,priv->site_survey.bss_backup[idx].ssid);
			found = 1;
		}			
	}

	if(found==0){
		P2P_DEBUG("can't chk dev's role\n");
		return 0;
	}else{
		P2P_DEBUG("Target role = %s \n",(priv->p2pPtr->target_device_role==R_P2P_GO?"GO":"dev"));
		
	}	

	// 0329
	if(P2PMODE == P2P_CLIENT || P2PMODE == P2P_TMP_GO){
		P2P_DEBUG("client/GO don't send provision req\n");
		return 0;
	}

	/*stay at target dev's channel and wait for provision Rsp*/				
	priv->pshare->CurrentChannelBW = HT_CHANNEL_WIDTH_20;

	SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);				
	SwChnl(priv, priv->p2pPtr->target_device_channel, priv->pshare->offset_2nd_chan);
	P2P_DEBUG("Target device is under channel:%d\n",priv->p2pPtr->target_device_channel);

	
	/*issue privision req*/		
	if(p2p_issue_provision_req(priv)==SUCCESS){	//success				
		priv->p2pPtr->provision_req_timeout = 10;	// per 2second ; total 5times (include first time)			
	}else{
		P2P_DEBUG("issue_provision_req fail\n");
		P2P_DEBUG("->\n");		
		P2P_listen(priv,NULL);
	}
	
	return 0;	
}



int p2p_issue_provision_req(struct rtl8192cd_priv *priv )
{

	unsigned char	*pbuf;
	unsigned char 	 randomX=0;

	unsigned char	*IEStart;
	unsigned char	*thisAttrPrt;
	unsigned char	 byteVal;
	unsigned short	 shortVal;
	unsigned char	 p2pIELen = 0;	
	unsigned char	*P2PIEstart=NULL;	
	//P2P_DEBUG("\n");	
	
	DECLARE_TXINSN(txinsn);

	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = _6M_RATE_;
	txinsn.lowest_tx_rate = txinsn.tx_rate;
	txinsn.fixed_rate = 1;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_provosionreq_fail1;

	txinsn.phdr = get_wlanhdr_from_poll(priv);

	if (txinsn.phdr == NULL)
		goto issue_provosionreq_fail1;


	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));


	pbuf[0] = CATEGORY_P2P_PUBLIC_ACTION;
	pbuf[1] = ACTIONY_P2P_PUBLIC_ACTION;
	memcpy(&pbuf[2],WFA_OUI_PLUS_TYPE,4);
	pbuf[6] = P2P_PROV_DISC_REQ;	

	get_random_bytes(&randomX , 1);		

	priv->p2pPtr->provision_tx_dialog_token = randomX;	



	pbuf[7] = priv->p2pPtr->provision_tx_dialog_token;

	/*below start is WSC IE content*/
	
	/*Element ID*/ 
	thisAttrPrt = pbuf + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	IEStart = pbuf + _P2P_PUBLIC_ACTION_IE_OFFSET_;	//anchor


	thisAttrPrt[0] = WSC_IE_ID;
	thisAttrPrt+=2;  // ID + LEN
	//thisAttrPrt[1] = WSC IE length;

	/*OUI*/ 	
	memcpy(thisAttrPrt, WSC_IE_OUI, 4);
	thisAttrPrt += 4; // OUI LEN
	
	/*Version*/ 
	byteVal = WSC_VER;
	thisAttrPrt = wsc_add_tlv(thisAttrPrt, TAG_VERSION, 1, (void *)&byteVal);

	shortVal = priv->p2pPtr->wsc_method_to_target_dev ;
	thisAttrPrt = wsc_add_tlv(thisAttrPrt, TAG_CONFIG_METHODS, 2, (void *)&shortVal);
	/*Element Length*/ 
	IEStart[1] = (int)(((unsigned long)thisAttrPrt)-((unsigned long)IEStart)) - 2;

	/*end of  WSC IE content*/

	if(priv->p2pPtr->target_device_role == R_P2P_GO){
		p2pIELen = 0;
		P2PIEstart = thisAttrPrt;
		thisAttrPrt[0] = P2P_IE_ID;
		thisAttrPrt+=2;  // ID + LEN

		/*P2P OUI+TYPE*/
		memcpy(thisAttrPrt, WFA_OUI_PLUS_TYPE, 4);
		thisAttrPrt+=4; // OUI LEN
		p2pIELen +=4;
		
		/*p2p device id ; ID2 */
		thisAttrPrt = add_Attr_capability( priv, thisAttrPrt, &p2pIELen,NULL);		

		/*p2p group id ; ID13 */
		thisAttrPrt = add_Attr_device_info( priv, thisAttrPrt, &p2pIELen);

		/*p2p group id ; ID15 */
		thisAttrPrt = add_Attr_Target_group_id( priv, thisAttrPrt, 
			&p2pIELen,priv->p2pPtr->target_device_ssid,priv->p2pPtr->target_device_addr);

		P2PIEstart[1] = p2pIELen;
	}	

	



	txinsn.fr_len += ((int)(((unsigned long)thisAttrPrt)-((unsigned long)pbuf)));

	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), priv->p2pPtr->target_device_addr, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	


	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS){

		P2P_PRINT("\n\n");		
		P2P_DEBUG("TX Provision_Req OK (My method=%02x)\n",shortVal);
		MAC_PRINT(priv->p2pPtr->target_device_addr);

		
		P2P_STATE = P2P_S_PROVI_WAIT_RSP;		
		return SUCCESS;
	}

issue_provosionreq_fail1:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	return FAIL;

	
}



void p2p_on_provision_rsp(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo)
{
	/*start of action frame content*/
	unsigned char *pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	

	int tmplen=0;			
	int foundWSCIE = 0;
	unsigned short wscMethod=0;	
	
	unsigned char* tmpPtr=NULL;	
	unsigned char *wscIEPtr = NULL ;	
	int wscIElen=0;	

	if(pframe[7] != priv->p2pPtr->provision_tx_dialog_token  )
		P2P_DEBUG("provision_req dialog_token=%d , no match\n",priv->p2pPtr->provision_rx_dialog_token);

	//==========================================================================
	//==========================================================================
	/*get WSC IE*/ 
	tmpPtr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	
	for(;;)
	{
		tmpPtr = get_ie(tmpPtr, _WPS_IE_, &tmplen,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
	
		if(tmpPtr!=NULL){
			if (!memcmp(tmpPtr+2, WSC_IE_OUI, 4)) {
				wscIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				wscIEPtr = tmpPtr+6 ;
				foundWSCIE = 1;				
				break;
			}
		}else{
			break ;
		}
		tmpPtr = tmpPtr + tmplen +2 ;		
	}

	if(foundWSCIE){
		tmpPtr = search_wsc_tag(wscIEPtr, TAG_CONFIG_METHODS, wscIElen, &tmplen);		
		if(tmpPtr){
			wscMethod = *(unsigned short*)tmpPtr;
			if(wscMethod == priv->p2pPtr->wsc_method_to_target_dev){
				priv->p2pPtr->wsc_method_match = 1;
				P2P_DEBUG("wsc method=%02x match!!!\n",wscMethod);
			}else{
				P2P_DEBUG("wsc method=%02x no match!!!\n\n\n",wscMethod);			
				priv->p2pPtr->wsc_method_match = 0;			
			}
		}
	}else{
		P2P_DEBUG("\n\n	no include TAG_CONFIG_METHODS\n\n");	
	}	
	//=========================================================================
	//==========================================================================
	
	priv->p2pPtr->provision_req_timeout = 0;
	
	// go back listen state
	P2P_DEBUG("->\n");	
	P2P_listen(priv,NULL);
	
	
}

#define PASSIVE_RX_provision_req	6768	// no meaning

void p2p_on_provision_req(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo)
{

	/*start of action frame content*/
	unsigned char *pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	
	int tmplen=0;	
	unsigned short wscMethod=0;	
	unsigned char* IEptr=NULL;	
	unsigned char *p2pIEPtr = NULL;
	int p2pIElen=0;
	int foundP2PIE = 0;	
	
	unsigned char *wscIE=NULL ;	// for ReAssembly	
	int wscIElen=0;	
	int foundWSCIE = 0;	

	/*when i am GO will use it*/
	struct p2p_device_peer *current_nego_peer=NULL; 	
	unsigned char *ptrtmp=NULL;
	int tag_len;		
	
	priv->p2pPtr->provision_rx_dialog_token = pframe[7];
	//P2P_DEBUG("\n");
	
	/*-----------------------------get WSC IE------------------------------*/ 
	IEptr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	
	for(;;)
	{
		IEptr = get_ie(IEptr, _WPS_IE_, &tmplen,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
	
		if(IEptr!=NULL){
			if (!memcmp(IEptr+2, WSC_IE_OUI, 4)) {
				wscIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				wscIE = IEptr+6 ;
				foundWSCIE = 1;				
				break;
			}
		}else{
			break ;
		}
		IEptr = IEptr + tmplen +2 ;		
	}
	if(foundWSCIE){	
		IEptr = search_wsc_tag(wscIE, TAG_CONFIG_METHODS, wscIElen, (int *)&tmplen);
		wscMethod = *(unsigned short*)IEptr;
		if(wscMethod){
			priv->p2pPtr->wsc_method_from_target_dev = wscMethod;			
			P2P_DEBUG("\n	WSC Method:%02x\n\n",wscMethod);						
		}
	}
	/*-----------------------------get WSC IE------------------------------*/ 

	
	/*-----------------------------get P2P IE(start)------------------------------*/ 
	IEptr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	
	for(;;)
	{
		IEptr = get_ie(IEptr, _P2P_IE_, &tmplen,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
	
		if(IEptr!=NULL){
			if (!memcmp(IEptr+2, WFA_OUI_PLUS_TYPE, 4)) {
				p2pIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				p2pIEPtr = IEptr+6 ;
				foundP2PIE = 1;				
				break;
			}
		}else{
			break ;
		}
		IEptr = IEptr + tmplen +2 ;		
	}
	
	if(foundP2PIE){	

		/*only GO need process P2P IE in provision req*/			
		if(P2PMODE == P2P_TMP_GO){
			/*only GO need process P2P IE in provision req*/
			current_nego_peer = &priv->p2pPtr->ongoing_nego_peer ;
			memset((void *)current_nego_peer , 0 ,sizeof(struct p2p_device_peer));
			
			/*ID2*/ 
			ptrtmp = p2p_search_tag(p2pIEPtr, p2pIElen,  TAG_P2P_CAPABILITY , &tag_len);
			if(ptrtmp){
				current_nego_peer->dev_capab = ptrtmp[0];
				current_nego_peer->group_capab= ptrtmp[1];			
			}

			if(ptrtmp[1] & BIT(0)){
				priv->p2pPtr->target_device_role = R_P2P_GO;
				P2P_DEBUG("\n	target dev should't  GO role ,chk!!!\n");
			}else{
				priv->p2pPtr->target_device_role = R_P2P_DEVICE;
			}


				
			/*ID13*/ 
			ptrtmp = p2p_search_tag(p2pIEPtr, p2pIElen,  TAG_P2P_DEVICE_INFO , &tag_len);
			if(ptrtmp){
				parse_device_info(ptrtmp , tag_len ,&current_nego_peer->peer_device_info);			
			}

			/*ID15*/ 
			ptrtmp = p2p_search_tag(p2pIEPtr, p2pIElen,  TAG_P2P_GROUP_ID , &tag_len);
			if(ptrtmp){
				parse_group_id(ptrtmp , tag_len ,current_nego_peer);
			}			

		}	
	}
	/*-----------------------------get P2P IE(end)------------------------------*/ 	


	/*for indicate UI that have provision Req*/
	P2P_EVENT_INDICATE = P2P_EVENT_RX_PROVI_REQ ;	 

	//P2P_DEBUG("\n");

	
	p2p_issue_provision_rsp(priv , pfrinfo->sa);
	
}



int p2p_issue_provision_rsp(struct rtl8192cd_priv *priv, unsigned char *da)
{


	unsigned char	*pbuf;

	unsigned char	*wscStart;
	unsigned char	*wscPtr;
	unsigned char	 byteVal;
	unsigned short	 shortVal;
	DECLARE_TXINSN(txinsn);
	P2P_DEBUG("\n");
	//P2P_PRINT("\n\n");
	//P2P_DEBUG("TX Provision RSP \n\n");
	
	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = _6M_RATE_;
	txinsn.lowest_tx_rate = txinsn.tx_rate;
	txinsn.fixed_rate = 1;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_provosionrsp_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto issue_provosionrsp_fail;

	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));
	//P2P_DEBUG("\n");


	pbuf[0] = CATEGORY_P2P_PUBLIC_ACTION;
	pbuf[1] = ACTIONY_P2P_PUBLIC_ACTION;
	memcpy(&pbuf[2],WFA_OUI_PLUS_TYPE,4);
	pbuf[6] = P2P_PROV_DISC_RSP;
	pbuf[7] = priv->p2pPtr->provision_rx_dialog_token;



	/*below start is WSC IE content*/
	
	/*Element ID*/ 
	wscPtr = pbuf + _P2P_PUBLIC_ACTION_IE_OFFSET_; 
	wscStart = pbuf + _P2P_PUBLIC_ACTION_IE_OFFSET_;	//anchor


	wscPtr[0] = WSC_IE_ID;

	wscPtr+=2;  // ID + LEN


	/*OUI*/ 	
	memcpy(wscPtr, WSC_IE_OUI, 4);
	wscPtr+=4; // OUI LEN
	
	/*Version*/ 
	byteVal = WSC_VER;
	wscPtr = wsc_add_tlv(wscPtr, TAG_VERSION, 1, (void *)&byteVal);


	/*config method*/
	shortVal = priv->p2pPtr->wsc_method_from_target_dev;
	wscPtr = wsc_add_tlv(wscPtr, TAG_CONFIG_METHODS, 2, (void *)&shortVal);
	//P2P_DEBUG("wsc method=%02x\n",priv->p2pPtr->wsc_method_from_target_dev);

	/*Element Length*/ 
	wscStart[1] = (int)(((unsigned long)wscPtr)-((unsigned long)wscStart)) - 2;

	txinsn.fr_len += ((int)(((unsigned long)wscPtr)-((unsigned long)pbuf)));

	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), da, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	
	//P2P_DEBUG("\n");

	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS){
		//P2P_DEBUG("\n");
		return SUCCESS;
	}

issue_provosionrsp_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	return FAIL;
}

#define ACTIVE_TX_GO_NEGO_REQ	0	// no meaning


/* plus remember to move */
int req_p2p_wsc_confirm(struct rtl8192cd_priv *priv, unsigned char *data)
{
	
	struct __p2p_wsc_confirm *wsc_confirm_ptr = (struct __p2p_wsc_confirm *)data;

	struct p2p_device_peer wsc_peer_t;


	// for debug
	P2P_DEBUG("pincode=[%s]\n",wsc_confirm_ptr->pincode);
	P2P_DEBUG("wscMethod=[%02x], from ",wsc_confirm_ptr->wsc_config_method);	
	MAC_PRINT(wsc_confirm_ptr->dev_address);		
	// for debug

	/*get PIN code*/ 
	strcpy(priv->p2pPtr->target_dev_pin_code,wsc_confirm_ptr->pincode);

	if(priv->p2pPtr->wsc_method_match==0){
		P2P_DEBUG("Passive mode,Confirm wsc method\n");					
		if(wsc_confirm_ptr->wsc_config_method == CONFIG_METHOD_PBC){
			P2P_DEBUG("PBC,Confirm!!\n");
			priv->p2pPtr->passivemode_pbc_trigger_flag = 1;
		}				
	}



	if(priv->p2pPtr->target_device_role == R_P2P_DEVICE && P2PMODE==P2P_DEVICE){
		/* 1)target is dev   and   i am dev  & 2) i issue this nego req*/

		P2P_DEBUG("\n	Target is dev ;  i am dev \n");
		    /*Use priv->p2pPtr->wsc_method_match for identify 
			   i am active send nego req or not */
			   
		if(priv->p2pPtr->wsc_method_match==0){
			P2P_DEBUG("Passive mode,Confirm wsc method\n");
			return 0;
		}else{
			P2P_DEBUG("Ative mode,Prepare send Go Nego Req!!\n");		
			priv->p2pPtr->wsc_method_match=0;
		}
	
		if(memcmp(priv->p2pPtr->target_device_addr ,wsc_confirm_ptr->dev_address ,6 )){
			P2P_DEBUG("dev addr no match, chk!!!!\n\n\n");
			return 0;
		}
		if(is_zero_ether_addr(wsc_confirm_ptr->dev_address )){
			P2P_DEBUG("dev addr is zero !!!\n\n\n");
			return 0;
		}
	
		/* switch channel to target dev's channel first */	
		priv->pshare->CurrentChannelBW = HT_CHANNEL_WIDTH_20;
		SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);		
		SwChnl(priv, priv->p2pPtr->target_device_channel, priv->pshare->offset_2nd_chan);

		//P2P_DEBUG("Rx wsc confirm ; start Tx nego Req\n");			
		if(p2p_issue_GO_nego_req(priv)==SUCCESS){		
			priv->p2pPtr->go_nego_on_going = 1;	// and P2P_STATE = P2P_S_NEGO_WAIT_RSP;
			priv->p2pPtr->go_nego_on_going_timeout = 10 ;	
			
		}

	}
	else if(priv->p2pPtr->target_device_role == R_P2P_DEVICE && P2PMODE==P2P_TMP_GO){	

		P2P_DEBUG("\n\ntarget is device   and   i am  GO \n");
		/*target is device   and   i am  GO , wsc method and pin etc is rdy ,*/		
		memset(&wsc_peer_t , 0 , sizeof(struct p2p_device_peer));

		if(priv->p2pPtr->wsc_method_from_target_dev==CONFIG_METHOD_KEYPAD){

			/*target will display ; use Target's PIN*/ 
			wsc_peer_t.device_pass_id = PASS_ID_REG;			
			
		}else if(priv->p2pPtr->wsc_method_from_target_dev==CONFIG_METHOD_PIN ||
			priv->p2pPtr->wsc_method_from_target_dev==CONFIG_METHOD_DISPLAY){

			/*target will keyin pin ; use My PIN */ 
			wsc_peer_t.device_pass_id = PASS_ID_USER;									
			
		}else if(priv->p2pPtr->wsc_method_from_target_dev==CONFIG_METHOD_PBC){

			/*PBC*/
			wsc_peer_t.device_pass_id = PASS_ID_PB;		
			
			
		}else{
			P2P_DEBUG("unknow type !!!\n\n");
		}



		/*indicate wscd satrt WPS procotol*/ 
		indicate_wscd(priv, MODE_AP_PROXY_REGISTRAR , priv->p2pPtr->go_PSK , &wsc_peer_t);			
			

	}
	else if(priv->p2pPtr->target_device_role == R_P2P_GO && P2PMODE == P2P_DEVICE){

		/*target is GO   and   i am  device*/

		P2P_DEBUG("\n\ntarget is GO   and   i am  device \n");
		P2P_DEBUG("\n change to  Pre-client and start WPS ,\n");	
		p2p_as_preClient(priv);

		memset(&wsc_peer_t , 0 , sizeof(struct p2p_device_peer));
		if(priv->p2pPtr->wsc_method_to_target_dev == CONFIG_METHOD_KEYPAD){
			/*tell target to keyin ; use My PIN */ 
			wsc_peer_t.device_pass_id = PASS_ID_USER;
			
		}else if(priv->p2pPtr->wsc_method_to_target_dev==CONFIG_METHOD_PIN ||
			priv->p2pPtr->wsc_method_to_target_dev==CONFIG_METHOD_DISPLAY){

			/*tell target to display ; use Target PIN */ 
			wsc_peer_t.device_pass_id = PASS_ID_REG;			

			
		}else if(priv->p2pPtr->wsc_method_to_target_dev==CONFIG_METHOD_PBC){
			/* PBC */ 
			wsc_peer_t.device_pass_id = PASS_ID_PB;		
			
		}else{
			P2P_DEBUG("unknow type !!!\n\n");
		}		

		/* indicate wscd change mode and start WPS under client mode */ 
		indicate_wscd(priv, MODE_CLIENT_UNCONFIG , NULL, &wsc_peer_t);
		priv->p2pPtr->pre_client_timeout = WSC_MODE_WAIT_TIME ;					


	}
	else{
		P2P_DEBUG("unknow type chk!!!\n\n");
	}

	return 0;
}

int p2p_issue_GO_nego_req(struct rtl8192cd_priv *priv)
{


	unsigned char	*pbuf;
	unsigned char *thisAttrPrt=NULL;
	unsigned char p2pIELen = 0;	

	unsigned char	*tmpPtr=NULL;	
	unsigned short	 shortVal;
	unsigned char	StrTmp2[40];		
	unsigned char My_tie_break;
	unsigned char randomX;
	
	DECLARE_TXINSN(txinsn);
	
	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = _6M_RATE_;
	txinsn.lowest_tx_rate = txinsn.tx_rate;
	txinsn.fixed_rate = 1;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_nego_req_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto issue_nego_req_fail;

	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));



	pbuf[0] = CATEGORY_P2P_PUBLIC_ACTION;
	pbuf[1] = ACTIONY_P2P_PUBLIC_ACTION;
	memcpy(&pbuf[2],WFA_OUI_PLUS_TYPE,4);
	pbuf[6] = P2P_GO_NEG_REQ;

	get_random_bytes(&randomX , 1);		
	priv->p2pPtr->go_nego_tx_dialog_token = randomX;		
	pbuf[7] = randomX;

	/* ===== WSC IE content =========*/	
	shortVal = priv->p2pPtr->dev_passwd_to_tar_dev;
	
	if(priv->p2pPtr->dev_passwd_to_tar_dev == PASS_ID_REG){		
		P2P_DEBUG("use My PIN \n");		
		strcpy(StrTmp2,"PASS_ID_REG");
	}else if(priv->p2pPtr->dev_passwd_to_tar_dev == PASS_ID_USER){
		P2P_DEBUG("use Target's PIN \n");		
		strcpy(StrTmp2,"PASS_ID_USER");	
	}else if(priv->p2pPtr->dev_passwd_to_tar_dev == PASS_ID_PB){	
		strcpy(StrTmp2,"PASS_ID_PB");		
	}else{
		P2P_DEBUG("unknow type !!!\n\n");	
	}


	priv->p2pPtr->wsc_ie_req_mun = wsc_build_probe_rsp_ie(priv, priv->p2pPtr->wsc_ie_req ,shortVal);			

	memcpy(&pbuf[8] , priv->p2pPtr->wsc_ie_req , priv->p2pPtr->wsc_ie_req_mun);
	//p2p_debug_out("GO Nego Req ; WSC IE", priv->p2pPtr->wsc_ie_req ,priv->p2pPtr->wsc_ie_req_mun);	
	/* ===== WSC IE content =========*/
	
	/* ===== P2P IE content =========*/	
	thisAttrPrt = pbuf + 8 + priv->p2pPtr->wsc_ie_req_mun ;
	tmpPtr = pbuf + 8 + priv->p2pPtr->wsc_ie_req_mun ;
	thisAttrPrt[0]=_P2P_IE_;

	/*fill p2p ie total length later(size == 2bytes)*/
	thisAttrPrt+=2;
	
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen+=4;
	thisAttrPrt+=4;


	/*id2*/
	thisAttrPrt = add_Attr_capability( priv, thisAttrPrt, &p2pIELen,NULL);


	get_random_bytes(&My_tie_break , 1);	
	My_tie_break %= 2;
	P2P_DEBUG("(Nego req)Method type:%s,tie_break=%d\n",StrTmp2,My_tie_break);	
	/* id4*/
	thisAttrPrt = add_Attr_GO_intent( priv, thisAttrPrt, &p2pIELen ,My_tie_break);

	/* id5*/
	thisAttrPrt = add_Attr_configration_timeout( priv, thisAttrPrt, &p2pIELen ,1);

	/* id6*/
	thisAttrPrt = add_Attr_listenChannel( priv, thisAttrPrt, &p2pIELen );

	/* id8*/
	thisAttrPrt = add_Attr_extended_listen_timing( priv, thisAttrPrt, &p2pIELen ,65535,65535);


	/* id9*/
	thisAttrPrt = add_Attr_intended_interface( priv, thisAttrPrt, &p2pIELen);

	/* id11*/
	thisAttrPrt = add_Attr_channel_list( priv, thisAttrPrt, &p2pIELen);	
	
	/* id13 */
	thisAttrPrt = add_Attr_device_info( priv, thisAttrPrt, &p2pIELen);
	

	/* id17*/
	thisAttrPrt = add_Attr_operation_channel( priv, thisAttrPrt, &p2pIELen);



	tmpPtr[1]=p2pIELen;

	//p2p_debug_out("GO Nego Req ; P2P IE", tmpPtr,p2pIELen );


	/* ===== total LEN =========*/
	txinsn.fr_len += ((int)(((unsigned long)thisAttrPrt)-((unsigned long)pbuf)));

	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), priv->p2pPtr->target_device_addr, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	


	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS){

		P2P_PRINT("\n");	
		P2P_DEBUG(" Tx GO nego Req OK\n\n");				
		P2P_STATE = P2P_S_NEGO_WAIT_RSP;
		return SUCCESS;
	}

issue_nego_req_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	return FAIL;

}






void p2p_on_GO_nego_rsp(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo)
{

	/*start of action frame content*/
	unsigned char *pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	
	unsigned char *ptrtmp = NULL;
	unsigned char* IEptr=NULL;	
	int tmplen=0;	
	unsigned char *p2pIE;
	int p2pIElen=0;
	int foundP2PIE = 0;	

	unsigned char *wscIE;
	int wscIElen=0;		
	int foundWSCIE = 0;
	int tag_len;	
	struct p2p_device_peer *current_nego_peer=NULL; 	

	if(priv->p2pPtr->go_nego_on_going && P2P_STATE == P2P_S_NEGO_WAIT_RSP){	

		current_nego_peer = &priv->p2pPtr->ongoing_nego_peer ;		
		memset(current_nego_peer , 0 , sizeof(struct p2p_device_peer));			

		
	}else{
		if(P2P_STATE != P2P_S_NEGO_WAIT_RSP)	
		P2P_DEBUG("rx nego rsp ; but not at P2P_S_NEGO_WAIT_RSP\n");	
		return ;		
	}

	current_nego_peer->dialog_token = pframe[7];
	if(pframe[7] != priv->p2pPtr->go_nego_tx_dialog_token){
		P2P_DEBUG("go_nego_tx_dialog_token no match!!\n\n\n");	
	}
	
	memcpy(current_nego_peer->dev_addr,pfrinfo->sa,6);
	//==========================================================================
	/*get WSC IE*/ 
	IEptr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	
	for(;;)
	{
		IEptr = get_ie(IEptr, _WPS_IE_, &tmplen,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
	
		if(IEptr!=NULL){
			if (!memcmp(IEptr+2, WSC_IE_OUI, 4)) {
				wscIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				wscIE = IEptr+6 ;
				foundWSCIE = 1;				
				break;
			}
		}else{
			break ;
		}
		IEptr = IEptr + tmplen +2 ;		
	}
	//==========================================================================
	if(foundWSCIE){
		ptrtmp = search_wsc_tag(wscIE, TAG_DEVICE_PASSWORD_ID, wscIElen, &tag_len);		
		if(ptrtmp){
			current_nego_peer->device_pass_id = *(unsigned short *)ptrtmp;
		}
	}else{
		current_nego_peer->status = P2P_SC_FAIL_INCOMPATIBLE_PROV_METHOD;
		P2P_DEBUG("\n\n	no include DEVICE PASS ID\n\n");	
	}	
	//==========================================================================
	
	//==========================================================================
	/*get P2P IE*/ 
	IEptr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	for(;;){

		IEptr = get_ie(IEptr, _P2P_IE_, &tmplen,
				pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
		
		if(IEptr!=NULL){
			if (!memcmp(IEptr+2, WFA_OUI_PLUS_TYPE, 4)) {
				p2pIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				p2pIE = IEptr+6 ;
				foundP2PIE = 1;
				//p2p_debug_out("P2P IE", p2pIE, p2pIElen);	
				break;				
			}
		}else{
			break ;
		}
		IEptr = IEptr + tmplen +2 ;		
	}
	
	if(foundP2PIE){

		// ID0
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_STATUE , &tag_len);
		if(ptrtmp){
			current_nego_peer->status = ptrtmp[0];
			if(current_nego_peer->status){
				P2P_DEBUG("nego fail!! report from target ; status = ( %d ) \n",
					current_nego_peer->status);

				P2P_STATE = P2P_S_LISTEN;				
				return;
			}

		}
		
		// ID2
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_P2P_CAPABILITY , &tag_len);
		if(ptrtmp){
			current_nego_peer->dev_capab = ptrtmp[0];
			current_nego_peer->group_capab= ptrtmp[1];			
		}

		// ID4
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_GROUP_OWNER_INTENT , &tag_len);
		if(ptrtmp){
			current_nego_peer->intent_value = ptrtmp[0]>>1;
			current_nego_peer->TieBreak = ptrtmp[0]&0x01;			
			//P2P_DEBUG("Intent T(%d),M(%d) \n",
			//	current_nego_peer->intent_value,priv->pmib->p2p_mib.p2p_intent);			
		}

		// ID5
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_CONFIG_TIMEOUT , &tag_len);
		if(ptrtmp){
			current_nego_peer->go_config_timeout = ptrtmp[0];
			current_nego_peer->client_config_timeout = ptrtmp[1];
		}
		


		// ID9
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_INTEN_P2P_INTERFACE_ADDR , &tag_len);
		if(ptrtmp){
			memcpy(current_nego_peer->p2p_interface_address , ptrtmp ,6);
		}

		// ID11
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_CHANNEL_LIST , &tag_len);
		if(ptrtmp){
			parse_channel_list(ptrtmp , tag_len ,current_nego_peer);
		}		

		// ID13
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_P2P_DEVICE_INFO , &tag_len);
		if(ptrtmp){
			parse_device_info(ptrtmp , tag_len ,&current_nego_peer->peer_device_info);			
		}

		// ID15
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_P2P_GROUP_ID , &tag_len);
		if(ptrtmp){
			//P2P_DEBUG("TAG_P2P_GROUP_ID; len = %d \n",tag_len);			
			parse_group_id(ptrtmp , tag_len ,current_nego_peer);
		}		

		// ID17
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_OPERATION_CHANNEL , &tag_len);
		if(ptrtmp){
			memcpy(current_nego_peer->op_country , ptrtmp ,3);
			current_nego_peer->op_country[3]='\0';
			current_nego_peer->op_class = ptrtmp[3];
			current_nego_peer->op_channel = ptrtmp[4];
		}			
		
	}

	/*determine who is GO/Client by intent value*/
	if((priv->pmib->p2p_mib.p2p_intent==15)
		&& current_nego_peer->intent_value ==15){
		current_nego_peer->status = P2P_SC_FAIL_BOTH_GO_INTENT_15;
	}
	if(priv->pmib->p2p_mib.p2p_intent >  current_nego_peer->intent_value){
		current_nego_peer->role	= R_P2P_CLIENT;					
	}else if((priv->pmib->p2p_mib.p2p_intent ==  current_nego_peer->intent_value)){
	
		if(current_nego_peer->TieBreak){
			current_nego_peer->role	= R_P2P_GO;			
		}else{
			current_nego_peer->role	= R_P2P_CLIENT;			
		}
	}else if(priv->pmib->p2p_mib.p2p_intent <  current_nego_peer->intent_value){

		current_nego_peer->role	= R_P2P_GO;					
	}
	
	p2p_issue_GO_nego_conf(priv,current_nego_peer);

}


int p2p_issue_GO_nego_conf(struct rtl8192cd_priv *priv,
		struct p2p_device_peer *current_nego_peer)
{
	unsigned char	*pbuf;
	unsigned char *thisAttrPrt=NULL;
	unsigned char p2pIELen = 0;	
	unsigned char	*tmpPtr=NULL;		
	DECLARE_TXINSN(txinsn);

	
	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = _6M_RATE_;
	txinsn.lowest_tx_rate = txinsn.tx_rate;
	txinsn.fixed_rate = 1;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_nego_conf_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto issue_nego_conf_fail;

	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));



	pbuf[0] = CATEGORY_P2P_PUBLIC_ACTION;
	pbuf[1] = ACTIONY_P2P_PUBLIC_ACTION;
	memcpy(&pbuf[2],WFA_OUI_PLUS_TYPE,4);
	pbuf[6] = P2P_GO_NEG_CONF;
	pbuf[7] = current_nego_peer->dialog_token;

	/* ===== no need include WSC IE content =========*/


	/* ===== P2P IE content =========*/	
	thisAttrPrt = pbuf + 8 + priv->p2pPtr->wsc_ie_rsp_mun ;
	tmpPtr = pbuf + 8 + priv->p2pPtr->wsc_ie_rsp_mun ;
	thisAttrPrt[0]=_P2P_IE_;

	/*fill p2p ie total length later(size == 2bytes)*/
	thisAttrPrt+=2;
	
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen+=4;
	thisAttrPrt+=4;

	/*ID0*/
	thisAttrPrt = add_Attr_status(priv ,  thisAttrPrt, &p2pIELen ,current_nego_peer->status);

	/*ID2*/
	thisAttrPrt = add_Attr_capability( priv, thisAttrPrt, &p2pIELen,current_nego_peer);

	/* ID17*/
	if(current_nego_peer->role == R_P2P_CLIENT) // i am GO
		thisAttrPrt = add_Attr_operation_channel( priv, thisAttrPrt, &p2pIELen);

	/* ID11*/
	thisAttrPrt = add_Attr_channel_list( priv, thisAttrPrt, &p2pIELen);	
		
	/*ID15*/
	if(current_nego_peer->role == R_P2P_CLIENT) // i am GO
		thisAttrPrt = add_Attr_group_id( priv, thisAttrPrt, &p2pIELen);


	tmpPtr[1]=p2pIELen;

	//p2p_debug_out("p2p IE (nego Rsp)", tmpPtr,p2pIELen );


	/* ===== total LEN =========*/
	//txinsn.fr_len += priv->p2pPtr->wsc_ie_tmp_mun;
	txinsn.fr_len += ((int)(((unsigned long)thisAttrPrt)-((unsigned long)pbuf)));

	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), current_nego_peer->dev_addr, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
		
	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS){

		P2P_DEBUG("	TX  GO nego Conf ,OK!\n");

		if(priv->p2pPtr->go_nego_on_going){
			priv->p2pPtr->go_nego_on_going = 0;
		}
		
		priv->p2pPtr->requestor = 1;
		if(current_nego_peer->role == R_P2P_CLIENT){

			/*target dev will as  client  ; i will work as pre-GO*/
			P2P_DEBUG("	As Pre-GO role for %d seconds\n\n",WSC_MODE_WAIT_TIME);			
			p2p_as_GO(priv,P2P_PRE_GO);

			/*indicate wscd change mode ; and satrt WPS procotol*/ 
			indicate_wscd(priv, MODE_AP_PROXY_REGISTRAR , priv->p2pPtr->go_PSK , current_nego_peer);
			priv->p2pPtr->pre_go_timeout = WSC_MODE_WAIT_TIME ;					

		}
		else if(current_nego_peer->role == R_P2P_GO){

			P2P_DEBUG("	As Pre-client role for %d seconds\n\n",WSC_MODE_WAIT_TIME);
			p2p_as_preClient(priv);

			/*indicate wscd change mode ; and satrt WPS procotol*/ 
			indicate_wscd(priv, WPS_MODE_NO_CHANGE , NULL, current_nego_peer);
			priv->p2pPtr->pre_client_timeout = WSC_MODE_WAIT_TIME ;					
		}
		else{
			P2P_DEBUG("unknow type chk!!\n");
		}

		return SUCCESS ;
	}	
	
			

		
issue_nego_conf_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	return FAIL;
}


#define PASSIVE_RX_GO_NEGO_REQ	0	// no meaning




void p2p_on_GO_nego_req(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo)
{
	/*start of action frame content*/
	unsigned char *pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	
	unsigned char *ptrtmp = NULL;
	unsigned char* IEptr=NULL;	
	int tmplen=0;	

	unsigned char p2pIE[256];
	int p2pIElen=0;
	int foundP2PIE = 0;	

	//unsigned char wscIE[256];
	//int wscIElen=0;		
	int foundWSCIE = 0;
	int tag_len;		
	struct p2p_device_peer *current_nego_peer=NULL; 

	//P2P_DEBUG("\n\n");
	//P2P_STATE = P2P_S_NEGO_RX_REQ ;

	if(priv->p2pPtr->go_nego_on_going){	
		if(!memcmp(pfrinfo->sa , priv->p2pPtr->ongoing_nego_peer.dev_addr,6)){

			current_nego_peer = &priv->p2pPtr->ongoing_nego_peer ;
			memset(current_nego_peer , 0 , sizeof(struct p2p_device_peer));
			
		}else{
			current_nego_peer = &priv->p2pPtr->others_nego_tar_device ;
			current_nego_peer->status = P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE;		
		}
		
	}else{
		priv->p2pPtr->go_nego_on_going = 1;
		priv->p2pPtr->go_nego_on_going_timeout = 15;		
		//P2P_DEBUG("GO_nego with\n");
		//MAC_PRINT(pfrinfo->sa);

		current_nego_peer = &priv->p2pPtr->ongoing_nego_peer ;
		memset(current_nego_peer , 0 , sizeof(struct p2p_device_peer));
	}

	current_nego_peer->dialog_token = pframe[7];
	memcpy(current_nego_peer->dev_addr,pfrinfo->sa,6);
	//==========================================================================
	/*get WSC IE*/ 
	IEptr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	
	for(;;)
	{
		IEptr = get_ie(IEptr, _WPS_IE_, &tmplen,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
	
		if(IEptr!=NULL){
			if (!memcmp(IEptr+2, WSC_IE_OUI, 4)) {
				//wscIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				//memcpy(wscIE, IEptr+6, wscIElen);			
				foundWSCIE = 1;				
				break;
				//p2p_debug_out("WSC IE", wscIE, wscIElen);			
			}
		}else{
			break ;
		}
		IEptr = IEptr + tmplen +2 ;		
	}
	//==========================================================================
	if(foundWSCIE){
		ptrtmp = search_wsc_tag(IEptr+6, TAG_DEVICE_PASSWORD_ID, tmplen-4, &tag_len);		
		if(ptrtmp){
			current_nego_peer->device_pass_id = *(unsigned short *)ptrtmp;
			//P2P_DEBUG("\n\nDEVICE PASS ID:%02x\n\n",current_nego_peer->device_pass_id);			
		}
	}else{
		current_nego_peer->status = P2P_SC_FAIL_INCOMPATIBLE_PROV_METHOD;

	}	
	//==========================================================================
	
	//==========================================================================
	/*get P2P IE*/ 
	IEptr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	for(;;){

		IEptr = get_ie(IEptr, _P2P_IE_, &tmplen,
				pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
		
		if(IEptr!=NULL){
			if (!memcmp(IEptr+2, WFA_OUI_PLUS_TYPE, 4)) {

				p2pIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				memcpy(p2pIE, IEptr+6, p2pIElen);			
				foundP2PIE = 1;
				break;
				//p2p_debug_out("P2P IE", p2pIE, p2pIElen);			
				
			}
		}else{
			break ;
		}
		IEptr = IEptr + tmplen +2 ;		
	}
	
	if(foundP2PIE){

		// ID2
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_P2P_CAPABILITY , &tag_len);
		if(ptrtmp){
			current_nego_peer->dev_capab = ptrtmp[0];
			current_nego_peer->group_capab= ptrtmp[1];			
		}

		// ID4
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_GROUP_OWNER_INTENT , &tag_len);
		if(ptrtmp){
			current_nego_peer->intent_value = ptrtmp[0]>>1;
			current_nego_peer->TieBreak = ptrtmp[0]&0x01;	
			P2P_DEBUG("Intent T(%d),M(%d),Tie break=%d \n",
				current_nego_peer->intent_value,
				priv->pmib->p2p_mib.p2p_intent,
				current_nego_peer->TieBreak);
			
		}
		
		// ID6		
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_LISTEN_CHANNEL , &tag_len);
		if(ptrtmp){

			current_nego_peer->operating_class = ptrtmp[3];
			current_nego_peer->listen_channle = ptrtmp[4];
		}

		// ID9
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_INTEN_P2P_INTERFACE_ADDR , &tag_len);
		if(ptrtmp){
			memcpy(current_nego_peer->p2p_interface_address , ptrtmp ,6);
		}

		// ID11
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_CHANNEL_LIST , &tag_len);
		if(ptrtmp){
			parse_channel_list(ptrtmp , tag_len ,current_nego_peer);
		}		

		// ID13
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_P2P_DEVICE_INFO , &tag_len);
		if(ptrtmp){
			parse_device_info(ptrtmp , tag_len ,&(current_nego_peer->peer_device_info));	
		}

		// ID17
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_OPERATION_CHANNEL , &tag_len);
		if(ptrtmp){
			memcpy(current_nego_peer->op_country , ptrtmp ,3);
			current_nego_peer->op_country[3]='\0';
			current_nego_peer->op_class = ptrtmp[3];
			current_nego_peer->op_channel = ptrtmp[4];
		}			
		
	}
	
	p2p_issue_GO_nego_rsp(priv,current_nego_peer);

}

int p2p_issue_GO_nego_rsp(struct rtl8192cd_priv *priv,
		struct p2p_device_peer *current_nego_peer)
{


	unsigned char	*pbuf;
	unsigned char *thisAttrPrt=NULL;
	unsigned char p2pIELen = 0;	
	unsigned char	*tmpPtr=NULL;	
	unsigned short	 shortVal=0;
	int MyRole=0;	//  GO:1  client:2	
	int My_tie_break=0;	
		
	DECLARE_TXINSN(txinsn);
	
	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = _6M_RATE_;
	txinsn.lowest_tx_rate = txinsn.tx_rate;
	txinsn.fixed_rate = 1;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_nego_rsp_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto issue_nego_rsp_fail;

	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));



	pbuf[0] = CATEGORY_P2P_PUBLIC_ACTION;
	pbuf[1] = ACTIONY_P2P_PUBLIC_ACTION;
	memcpy(&pbuf[2],WFA_OUI_PLUS_TYPE,4);
	pbuf[6] = P2P_GO_NEG_RESP;
	pbuf[7] = current_nego_peer->dialog_token;

	/* ===== WSC IE content =========*/
	
	if(current_nego_peer->device_pass_id == PASS_ID_USER){
		//P2P_DEBUG("use My PIN \n");		
		shortVal = PASS_ID_REG;
	}else if(current_nego_peer->device_pass_id == PASS_ID_REG){
		//P2P_DEBUG("use Target's PIN \n");		
		shortVal = PASS_ID_USER;
	}else if(current_nego_peer->device_pass_id == PASS_ID_PB){
	
		shortVal = PASS_ID_PB;
	}else{
		P2P_DEBUG("unknow type\n");
	}
	
	priv->p2pPtr->wsc_ie_rsp_mun = wsc_build_probe_rsp_ie(priv, priv->p2pPtr->wsc_ie_rsp ,shortVal);			

	memcpy(&pbuf[8] , priv->p2pPtr->wsc_ie_rsp , priv->p2pPtr->wsc_ie_rsp_mun);

	//p2p_debug_out("wsc ie at nego rsp", priv->p2pPtr->wsc_ie_tmp,priv->p2pPtr->wsc_ie_tmp_mun);
	/* ===== WSC IE content =========*/
	

	/*determine who is GO/Client by intent value---start*/


	if((priv->pmib->p2p_mib.p2p_intent==15)
		&& current_nego_peer->intent_value ==15){
		current_nego_peer->status = P2P_SC_FAIL_BOTH_GO_INTENT_15;
	}
	if(priv->pmib->p2p_mib.p2p_intent >  current_nego_peer->intent_value){
		MyRole = R_P2P_GO;
		current_nego_peer->role	= R_P2P_CLIENT;					
	}else if((priv->pmib->p2p_mib.p2p_intent ==  current_nego_peer->intent_value)){
	
		if(current_nego_peer->TieBreak){
			MyRole = R_P2P_CLIENT;
			My_tie_break=0;
			current_nego_peer->role	= R_P2P_GO;			
		}else{
			MyRole = R_P2P_GO;		
			My_tie_break=1;
			current_nego_peer->role	= R_P2P_CLIENT;			
		}
	}else if(priv->pmib->p2p_mib.p2p_intent <  current_nego_peer->intent_value){
		MyRole = R_P2P_CLIENT;
		current_nego_peer->role	= R_P2P_GO;					
	}

	/*determine who is GO/Client by intent value---end	*/


	
	/* ===== P2P IE content =========*/	
	thisAttrPrt = pbuf + 8 + priv->p2pPtr->wsc_ie_rsp_mun ;
	tmpPtr = pbuf + 8 + priv->p2pPtr->wsc_ie_rsp_mun ;
	thisAttrPrt[0]=_P2P_IE_;

	/*fill p2p ie total length later(size == 2bytes)*/
	thisAttrPrt+=2;
	
	memcpy(thisAttrPrt , WFA_OUI_PLUS_TYPE ,4);
	p2pIELen+=4;
	thisAttrPrt+=4;


	/*ID0*/
	if(current_nego_peer->device_pass_id == PASS_ID_REG){	/*use Target's PIN code*/ 
		if(is_zero_pin_code(priv->p2pPtr->target_dev_pin_code)){
			P2P_DEBUG("PIN not ready yet\n");			
			current_nego_peer->status = P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE;
		}
	}else 
	if(current_nego_peer->device_pass_id == PASS_ID_PB){	/*Target will use PBC*/ 
		if(priv->p2pPtr->passivemode_pbc_trigger_flag == 0){
			P2P_DEBUG("PBC not allow yet\n");
			current_nego_peer->status = P2P_SC_FAIL_INFO_CURRENTLY_UNAVAILABLE;
		}
		/*else{
			// clean PBC flag 
			P2P_DEBUG("clean PBC flag\n");			
			priv->p2pPtr->passivemode_pbc_trigger_flag = 0;
		}
		*/
	}

	
	thisAttrPrt = add_Attr_status(priv ,  thisAttrPrt, &p2pIELen ,current_nego_peer->status);


	/*ID2*/
	thisAttrPrt = add_Attr_capability( priv, thisAttrPrt, &p2pIELen,current_nego_peer);


	/* ID4*/
	thisAttrPrt = add_Attr_GO_intent( priv, thisAttrPrt, &p2pIELen ,My_tie_break);

	/* ID5*/
	thisAttrPrt = add_Attr_configration_timeout( priv, thisAttrPrt, &p2pIELen ,MyRole);

	/* ID17*/
	if(MyRole == R_P2P_GO)
		thisAttrPrt = add_Attr_operation_channel( priv, thisAttrPrt, &p2pIELen);

	/* ID9*/
	thisAttrPrt = add_Attr_intended_interface( priv, thisAttrPrt, &p2pIELen);

	/* ID11*/
	thisAttrPrt = add_Attr_channel_list( priv, thisAttrPrt, &p2pIELen);	
	
	/* ID13*/
	thisAttrPrt = add_Attr_device_info( priv, thisAttrPrt, &p2pIELen);
	
	/* ID15*/
	if(MyRole == R_P2P_GO)	
		thisAttrPrt = add_Attr_group_id( priv, thisAttrPrt, &p2pIELen);


	tmpPtr[1]=p2pIELen;

	//p2p_debug_out("p2p IE (nego Rsp)", tmpPtr,p2pIELen );


	/* ===== total LEN =========*/
	txinsn.fr_len += ((int)(((unsigned long)thisAttrPrt)-((unsigned long)pbuf)));

	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), current_nego_peer->dev_addr, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	


	
	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS){

		// send rsp ok  ;wait conf
		P2P_STATE = P2P_S_NEGO_WAIT_CONF ;
		priv->p2pPtr->wait_nego_conf_timeout = 2;

		P2P_DEBUG("TX  GO nego Rsp success\n");
		if(current_nego_peer->status!=P2P_SC_SUCCESS){

			P2P_DEBUG("Intent T(%d),M(%d)\n",
				current_nego_peer->intent_value , priv->pmib->p2p_mib.p2p_intent);
			
			//P2P_DEBUG("My Role:%s\n",(MyRole==R_P2P_GO?"GO":"Client"));			
			
			if(priv->p2pPtr->go_nego_on_going){
				P2P_DEBUG("status = %d\n",current_nego_peer->status);
				P2P_DEBUG("unlock go_nego_ongoing\n\n");
				priv->p2pPtr->go_nego_on_going = 0;
			}				
			
		}			
		
		return SUCCESS;
	}
	else{
		P2P_DEBUG("tx nego rsp fail\n");
	}

issue_nego_rsp_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	return FAIL;
}

void p2p_on_GO_nego_conf(struct rtl8192cd_priv *priv ,struct rx_frinfo *pfrinfo)
{
	/*start of action frame content*/
	unsigned char *pframe = get_pframe(pfrinfo) + WLAN_HDR_A3_LEN;	
	int tmplen=0;	
	unsigned char *ptrtmp = NULL;
	unsigned char* IEptr=NULL;	
	unsigned char p2pIE[256];	
	int p2pIElen=0;
	int foundP2PIE = 0;
	int tag_len;	
	
	struct p2p_device_peer *current_nego_peer=NULL; 
	P2P_PRINT("\n");
	P2P_DEBUG("RX GO_nego_conf\n\n");

	if(priv->p2pPtr->go_nego_on_going){	
		current_nego_peer = &priv->p2pPtr->ongoing_nego_peer ;				
	}else{
		P2P_DEBUG("no GO nego on going , drop confirm\n\n");
		return ;
	}

	if(P2P_STATE != P2P_S_NEGO_WAIT_CONF){
		P2P_DEBUG("rx conf but state is wrong!!\n");			
		return ;		
	}else{
		P2P_STATE = P2P_S_LISTEN;
	}
	
	if(current_nego_peer == NULL){
		P2P_DEBUG("\n");			
		return ;		
	}

	/* check dialog_token */
	if(current_nego_peer->dialog_token != pframe[7]){
		P2P_DEBUG("dialog_token no match!!!\n\n");
		return ;
	}
		
	//==========================================================================
	/*get P2P IE*/ 
	IEptr = pframe + _P2P_PUBLIC_ACTION_IE_OFFSET_;
	tmplen = 0;
	for(;;){

		IEptr = get_ie(IEptr, _P2P_IE_, &tmplen,
				pfrinfo->pktlen - WLAN_HDR_A3_LEN - _P2P_PUBLIC_ACTION_IE_OFFSET_ - tmplen);		
		
		if(IEptr!=NULL){
			if (!memcmp(IEptr+2, WFA_OUI_PLUS_TYPE, 4)) {

				p2pIElen = tmplen-4 ; //skip WSC_IE_OUI(len==4)
				memcpy(p2pIE, IEptr+6, p2pIElen);			
				foundP2PIE = 1;
				//p2p_debug_out("P2P IE", p2pIE, p2pIElen);			
				break;
				
			}
		}else{
			break ;
		}
		IEptr = IEptr + tmplen +2 ;		
	}

	//==========================================================================	
	if(foundP2PIE){

		// ID0
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_STATUE , &tag_len);
		if(ptrtmp){
			current_nego_peer->status = ptrtmp[0];
			if(current_nego_peer->status){
				P2P_DEBUG("nego fail!! report from target ; status = ( %d ) \n\n",
					current_nego_peer->status);
				return;
			}

		}
		
		// ID2
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_P2P_CAPABILITY , &tag_len);
		if(ptrtmp){
			current_nego_peer->dev_capab = ptrtmp[0];
			current_nego_peer->group_capab= ptrtmp[1];			
		}

		// ID17
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_OPERATION_CHANNEL , &tag_len);
		if(ptrtmp){
			memcpy(current_nego_peer->op_country , ptrtmp ,3);
			current_nego_peer->op_country[3]='\0';
			current_nego_peer->op_class = ptrtmp[3];
			current_nego_peer->op_channel = ptrtmp[4];
		}			
	
		// ID11
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_CHANNEL_LIST , &tag_len);
		if(ptrtmp){
			parse_channel_list(ptrtmp , tag_len ,current_nego_peer);
		}		

		// ID15
		ptrtmp = p2p_search_tag(p2pIE, p2pIElen,  TAG_P2P_GROUP_ID , &tag_len);
		if(ptrtmp){
			//P2P_DEBUG("TAG_P2P_GROUP_ID; len = %d \n",tag_len);			
			parse_group_id(ptrtmp , tag_len ,current_nego_peer);
		}

		
	}


	priv->p2pPtr->requestor = 0;

	// nego success
	if(current_nego_peer->status == P2P_SC_SUCCESS)
	{
		
		if(current_nego_peer->role == R_P2P_CLIENT){

			/*target dev will as  client  ; i will work as pre-GO*/
			P2P_DEBUG("	As Pre-GO role for %d seconds\n\n",WSC_MODE_WAIT_TIME);						
			p2p_as_GO(priv,P2P_PRE_GO);
	
			// indicate wscd change mode ; and satrt WPS procotol
			indicate_wscd(priv, MODE_AP_PROXY_REGISTRAR , priv->p2pPtr->go_PSK , current_nego_peer);
			priv->p2pPtr->pre_go_timeout = WSC_MODE_WAIT_TIME ;			

		}
		else if(current_nego_peer->role == R_P2P_GO){
			/*i will work as pre-client*/
			P2P_DEBUG("	As Pre-client role for %d seconds\n\n",WSC_MODE_WAIT_TIME);
			p2p_as_preClient(priv);

			// indicate wscd change mode and start WPS

			/*start to wsc under enrollee mode*/
			indicate_wscd(priv, MODE_CLIENT_UNCONFIG , NULL, current_nego_peer);
			priv->p2pPtr->pre_client_timeout = WSC_MODE_WAIT_TIME ;
		}
		else{
			P2P_DEBUG("unknow type chk!!\n");
		}

	}	
	
	if(priv->p2pPtr->go_nego_on_going){	

		P2P_DEBUG("unlock go_nego_on_going (Rx nego conf) \n\n\n");
		priv->p2pPtr->go_nego_on_going = 0;
		
	}			
	//==========================================================================	
	return ;
	
}


#define EVENT_TO_WSCD	6768	// no meaning

void  stop_wscd(struct rtl8192cd_priv *priv ,unsigned char *data)
{

	DOT11_WSC_PIN_IND wsc_ind;

	if(	priv->p2pPtr->wps_is_ongoing != 1)
		return;
	
	wsc_ind.EventId = DOT11_EVENT_WSC_STOP;
	wsc_ind.IsMoreEvent = 0;

	P2P_DEBUG("\n");
	
	DOT11_EnQueue((unsigned long)priv, priv->pevent_queue,
		(unsigned char*)&wsc_ind, sizeof(DOT11_WSC_PIN_IND));
	event_indicate(priv, NULL, 0);
	priv->p2pPtr->wps_is_ongoing = 0;
}

void indicate_wscd(struct rtl8192cd_priv *priv , unsigned char mode ,
	unsigned char *PSK ,struct p2p_device_peer *nego_peer_xx)
{


	struct _DOT11_P2P_INDICATE_WSC	p2p_2_wsc_event_t;
	memset(&p2p_2_wsc_event_t , 0 ,sizeof(struct _DOT11_P2P_INDICATE_WSC));

	p2p_2_wsc_event_t.EventId = DOT11_EVENT_WSC_SWITCH_MODE;	// 100
	p2p_2_wsc_event_t.IsMoreEvent = 0;
	
	p2p_2_wsc_event_t.modeSwitch = mode;
	P2P_DEBUG("switch wps mode to %d\n",p2p_2_wsc_event_t.modeSwitch);

	if(PSK){
		strcpy(p2p_2_wsc_event_t.network_key , PSK);
		P2P_DEBUG("psk=%s\n",p2p_2_wsc_event_t.network_key);
	}else{
		p2p_2_wsc_event_t.network_key[0]='\0';
	}

	// gossid is use under i am go
	strcpy(p2p_2_wsc_event_t.gossid, priv->p2pPtr->my_GO_ssid);
	//P2P_DEBUG("gossid=%s\n",p2p_2_wsc_event_t.gossid);


	/*  If p2p_2_wsc_event_t.trigger_method no assigned ,
		wscd only change mode ,	will not trigger WPS protocol*/
	if(nego_peer_xx){

		if(nego_peer_xx->device_pass_id == PASS_ID_USER){

			// use My PIN 
			P2P_DEBUG("	use My PIN\n");
			p2p_2_wsc_event_t.whosPINuse = USE_MY_PIN ;
			p2p_2_wsc_event_t.trigger_method = P2P_PIN_METHOD;		 
			priv->p2pPtr->wps_is_ongoing = 1;				
			
		}else if(nego_peer_xx->device_pass_id == PASS_ID_REG){

			// use Target's PIN 
			P2P_DEBUG("	use Target's PIN=%s\n",priv->p2pPtr->target_dev_pin_code);
			p2p_2_wsc_event_t.whosPINuse = USE_TARGET_PIN;		
			p2p_2_wsc_event_t.trigger_method = P2P_PIN_METHOD;		 		
			strcpy(p2p_2_wsc_event_t.PINCode, priv->p2pPtr->target_dev_pin_code);
			priv->p2pPtr->wps_is_ongoing = 1;				
			
		}else if(nego_peer_xx->device_pass_id == PASS_ID_PB){
			P2P_DEBUG("	use PBC\n");		
			p2p_2_wsc_event_t.trigger_method = P2P_PBC_METHOD;		 		
			priv->p2pPtr->wps_is_ongoing = 1;
			
			if(	priv->p2pPtr->passivemode_pbc_trigger_flag )
				P2P_DEBUG("clean PBC flag\n");							
				priv->p2pPtr->passivemode_pbc_trigger_flag = 0;
			
		}else{
			P2P_DEBUG("	unknow type chk!!\n");
		}

	}

	if(priv->p2pPtr->requestor){
		P2P_DEBUG("	i am requestor\n");		
		p2p_2_wsc_event_t.requestor = 1;
	}else{
		P2P_DEBUG("	i am not requestor\n");		
	}
	
	
	DOT11_EnQueue((unsigned long)priv, priv->pevent_queue,
		(UINT8 *)&p2p_2_wsc_event_t, sizeof(struct _DOT11_P2P_INDICATE_WSC));

	event_indicate(priv, NULL, 0);

}


#define GROUP_SESSION_HANDLE	6768	// no meaning





void P2P_chk_assoc_client(struct rtl8192cd_priv *priv)
{

    int idx = 0;
    int found = 0;	
    int needrebuild = 0;		
	struct list_head *phead, *plist;
	struct stat_info *pstat;

	P2P_DEBUG("\n");

	if(	priv->assoc_num == 0){
		for(idx=0;idx<MAX_P2P_CLIENT_MUN;idx++){
			if(priv->p2pPtr->assocPeers[idx].inuse){

				memset(priv->p2pPtr->assocPeers, 0 ,sizeof(struct assoc_peer)*MAX_P2P_CLIENT_MUN);

				priv->p2pPtr->p2p_probe_rsp_ie_len = 
				p2p_build_probe_rsp_ie(priv,priv->p2pPtr->p2p_probe_rsp_ie);
				
				return;				
			}
		}		
	}

	phead = &priv->asoc_list;
	if (!netif_running(priv->dev) || list_empty(phead)) {
		return;
	}


	for(idx=0;idx<MAX_P2P_CLIENT_MUN;idx++){
		if(priv->p2pPtr->assocPeers[idx].inuse){

			found = 0;
			
			phead = &priv->asoc_list;			
			plist = phead->next;
			while (plist != phead) {
				
				pstat = list_entry(plist, struct stat_info, asoc_list);
				
				if(!memcmp(pstat->hwaddr ,priv->p2pPtr->assocPeers[idx].if_addr ,6))
					found = 1;
				
				plist = plist->next;
			}
			
			if(found==0){
				priv->p2pPtr->assocPeers[idx].inuse = 0;
				needrebuild = 1;
			}

		}
	}
		
	/*rebuild GO's probe_rsp content ,build Probe_Rsp P2P IE info */ 
	if(needrebuild){
		priv->p2pPtr->p2p_probe_rsp_ie_len = 
		p2p_build_probe_rsp_ie(priv,priv->p2pPtr->p2p_probe_rsp_ie);
	}
		
	return;
	
}

void P2P_client_on_beacon(struct rtl8192cd_priv *priv,
	unsigned char *IEaddr, unsigned int IElen, int seq)
{
	unsigned char* p2p_sub_ie=NULL;
	int tag_len = 0;		


	// ID 12
	p2p_sub_ie = p2p_search_tag(IEaddr, IElen, TAG_NOTICE_OF_ABSENCE, &tag_len);
	if (p2p_sub_ie) {
		//P2P_DEBUG("\n");		
		parse_p2p_NOA(priv, p2p_sub_ie, tag_len, seq);
	}
	else {
		//P2P_DEBUG("No TAG_NOTICE_OF_ABSENCE in beacon\n");
		p2p_cancel_noa(priv);
	}
	return;
}



int P2P_filter_manage_ap(struct rtl8192cd_priv *priv,
	unsigned char *IEaddr, unsigned int IElen )
{
	unsigned char* p2p_sub_ie=NULL;
	int tag_len = 0;		
	// ID10
	p2p_sub_ie = p2p_search_tag(IEaddr , IElen , TAG_P2P_MANAGEABILITY , &tag_len);
	
	if(p2p_sub_ie){
		P2P_DEBUG("\n");			
		if(p2p_sub_ie[0] & BIT(0))
			return 1;
		else
			return 0;
	}
		
	return 0;
	
}


void P2P_on_assoc_req(struct rtl8192cd_priv *priv,
	unsigned char *IEaddr, unsigned int IElen , unsigned char *sa)
{
	unsigned char* p2p_sub_ie;
	int tag_len = 0;		
    int idx = 0;
    int found = 0;	

	P2P_DEBUG("\n");
	
	for(idx=0;idx<MAX_P2P_CLIENT_MUN;idx++){
		if(priv->p2pPtr->assocPeers[idx].inuse){
			if(!memcmp(priv->p2pPtr->assocPeers[idx].if_addr,sa,6)){
				found = 1;
				break;
			}
		}
		if(priv->p2pPtr->assocPeers[idx].inuse == 0){
			priv->p2pPtr->assocPeers[idx].inuse = 1;
			found = 1;
			break;
		}
	}
	
	if(found==0){
		// second changes
		P2P_chk_assoc_client(priv);

		for(idx=0;idx<MAX_P2P_CLIENT_MUN;idx++){
			if(priv->p2pPtr->assocPeers[idx].inuse == 0){
				priv->p2pPtr->assocPeers[idx].inuse = 1;
				found = 1;
				break;
			}
		}		
		if(found==0){		
			P2P_DEBUG("assoc peer full!!,chk!!\n\n\n");
			return;
		}
	}

	// ID 2
	p2p_sub_ie = p2p_search_tag(IEaddr , IElen , TAG_P2P_CAPABILITY , &tag_len);
	if(p2p_sub_ie){
		priv->p2pPtr->assocPeers[idx].dev_cap = p2p_sub_ie[0];
		priv->p2pPtr->assocPeers[idx].group_cap = p2p_sub_ie[1];		
	}else{
		P2P_DEBUG("can't search TAG_P2P_CAPABILITY\n\n");
	}


	// ID 13
	p2p_sub_ie = p2p_search_tag(IEaddr , IElen , TAG_P2P_DEVICE_INFO , &tag_len);
	if(p2p_sub_ie){
		parse_device_info(p2p_sub_ie,tag_len, &(priv->p2pPtr->assocPeers[idx].devInfo));		
	}else{
		P2P_DEBUG("can't search TAG_P2P_DEVICE_INFO\n\n");
	}
	memcpy(priv->p2pPtr->assocPeers[idx].if_addr ,sa, 6);

	/*rebuild GO's probe_rsp content ,build Probe_Rsp P2P IE info */ 
	priv->p2pPtr->p2p_probe_rsp_ie_len = p2p_build_probe_rsp_ie(priv,priv->p2pPtr->p2p_probe_rsp_ie);
		
	return;
	
}



void P2P_on_assoc_rsp(struct rtl8192cd_priv *priv,unsigned char *sa)
{
	P2P_DEBUG("\n");
	memset(priv->p2pPtr->assocPeers,0,sizeof(struct assoc_peer)*MAX_P2P_CLIENT_MUN);			
	priv->p2pPtr->assocPeers[0].inuse = 1;
	memcpy(priv->p2pPtr->assocPeers[0].if_addr,sa,6);
	return;
	
}


void P2P_on_probe_req(
	struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo, 
	unsigned char *IEaddr, unsigned int IElen )
{

	unsigned char *pframe= get_pframe(pfrinfo);	
	unsigned char* pData=NULL;	
	int tag_len ;

	/*
	int lentmp=0;
	int wsc_ie_found=0;
	unsigned char *ptmp=NULL;	
	*/	

	P2P_PRINT("p2p probe_req from:");	
	MAC_PRINT(GetAddr2Ptr(pframe));

	/*	DA is broadcast addr or my p2p device addr 
		(default use MY-HW-ADDR as p2p device addrree)*/
	if( memcmp(GetAddr1Ptr(pframe), "\xff\xff\xff\xff\xff\xff", 6) 
		&& memcmp(GetAddr1Ptr(pframe), GET_MY_HWADDR, 6))
	{
		P2P_DEBUG("DA mismatch!\n");
		return;
	}
	
	/*chk BSSID(ADDR3) is broadcast MacAddr */
	if( memcmp(GetAddr3Ptr(pframe), "\xff\xff\xff\xff\xff\xff", 6) ){
		P2P_DEBUG("A3 mismatch!\n");
		return;
	}

	/*ID3*/
	pData = p2p_search_tag(IEaddr , IElen , TAG_DEVICE_ID , &tag_len);
	if(pData){
		if( memcmp(pData , GET_MY_HWADDR , 6)){
			/*this p2p IE include spec device addr for search , and target is not me*/ 
			return;	
		}
	}				

	#if 0	
	/*get WSC IE --start*/ 
	ptmp = pframe + WLAN_HDR_A3_LEN + _PROBEREQ_IE_OFFSET_; lentmp = 0;
	for (;;)
	{
		ptmp = get_ie(ptmp, _WPS_IE_, &lentmp, (int)(pfrinfo->pktlen - (ptmp - pframe)));
		if (ptmp != NULL) {
			if (!memcmp(ptmp+2, WSC_IE_OUI, 4)) {
				wsc_ie_found = 1;
				break;
			}
		}
		else
			break;

		ptmp = ptmp + lentmp + 2;
	}	
	if(wsc_ie_found){

		/*find TAG_PRIMARY_DEVICE_TYPE*/  
		pData = search_wsc_tag(ptmp+6, TAG_PRIMARY_DEVICE_TYPE, lentmp-4, &tag_len);
		if (pData == NULL) {
			P2P_DEBUG("Can't find TAG_PRIMARY_DEVICE_TYPE\n");
		}	
	}
	/*get WSC IE --end*/ 
	#endif
	
	if(P2PMODE ==P2P_TMP_GO){
		priv->p2pPtr->probe_rps_to_p2p_dev = 1;	
		P2P_DEBUG("GO Rsp p2p_probe_req\n");
		issue_probersp(priv, GetAddr2Ptr(pframe), SSID, SSID_LEN, FALSE);		
	}else{
		issue_probersp(priv, GetAddr2Ptr(pframe), P2P_WILDCARD_SSID, P2P_WILDCARD_SSID_LEN, FALSE);
	}
	
}


void p2p_client_remove(struct rtl8192cd_priv *priv , struct stat_info *pstat )
{
	
	int idx=0;
	if(P2PMODE!=P2P_TMP_GO){
		return;
	}
	P2P_DEBUG("\n");

	for(idx=0;idx<MAX_P2P_CLIENT_MUN;idx++){
		if(priv->p2pPtr->assocPeers[idx].inuse){
			if(!memcmp(priv->p2pPtr->assocPeers[idx].if_addr,pstat->hwaddr,6)){
				priv->p2pPtr->assocPeers[idx].inuse=0;
				priv->p2pPtr->p2p_probe_rsp_ie_len = 
					p2p_build_probe_rsp_ie(priv,priv->p2pPtr->p2p_probe_rsp_ie);				
				
				break;
			}		
		}
	}

	return;
}

#define SEPARATE4	0	// no meaning

void p2p_init(struct rtl8192cd_priv *priv )
{
	if(priv->p2pPtr == NULL){		
		P2P_DEBUG("first init kmalloc for p2p interface!\n");
		priv->p2pPtr = (struct p2p_context*)kmalloc(sizeof(struct p2p_context),GFP_ATOMIC);	
		if(priv->p2pPtr == NULL){
			P2P_DEBUG("\n\n!!!! chk here!!!\n\n\n\n\n");
		}else{			
			memset(priv->p2pPtr,0,sizeof(struct p2p_context));		
		}		
	}
		
	P2P_DEBUG("\n");	

	//memset(priv->p2pPtr->target_dev_pin_code,0,9);
	
	/*build beacon P2P IE */ 
	if(P2PMODE == P2P_TMP_GO || P2PMODE == P2P_PRE_GO ){
		memset(priv->p2pPtr->assocPeers,0,sizeof(struct assoc_peer)*MAX_P2P_CLIENT_MUN);		
	}
	
	/*build Probe_Rsp P2P IE info */ 
	priv->p2pPtr->p2p_probe_rsp_ie_len = 
		p2p_build_probe_rsp_ie(priv,priv->p2pPtr->p2p_probe_rsp_ie);


	/*build Probe_Req P2P IE info */ 
	priv->p2pPtr->p2p_probe_req_ie_len = 
		p2p_build_probe_req_ie(priv,priv->p2pPtr->p2p_probe_req_ie);



	/*build wsc IE info ;only use under P2P device mode ; under GO(AP) mode it should be
	fill by wscd , so take care!!*/ 
	if(P2PMODE == P2P_DEVICE){	
		priv->pmib->wscEntry.probe_rsp_ielen = 
		wsc_build_probe_rsp_ie(priv, priv->pmib->wscEntry.probe_rsp_ie , PASS_ID_REG);		
	}

	if(P2PMODE == P2P_CLIENT){	
		/*build assocReq P2P IE */ 
		priv->p2pPtr->p2p_assocReq_ie_len
			= p2p_build_assocReq_ie(priv,priv->p2pPtr->p2p_assocReq_ie);

		/*build disAssoc P2P IE */ 
		priv->p2pPtr->p2p_disass_ie_len
			= p2p_build_deassoc_ie(priv,priv->p2pPtr->p2p_disass_ie , minor_case1);
	
	}
	
	/*build beacon P2P IE */ 
	if(P2PMODE == P2P_TMP_GO || P2PMODE == P2P_PRE_GO ){

		priv->p2pPtr->p2p_beacon_ie_len = 
			p2p_build_beacon_ie(priv,priv->p2pPtr->p2p_beacon_ie);
		
		priv->p2pPtr->p2p_assoc_RspIe_len = 
			p2p_build_assocRsp_ie(priv,priv->p2pPtr->p2p_assoc_RspIe,P2P_SC_SUCCESS);
				
	}

								

	if(priv->p2pPtr->rdyinit){
			
	}else{
		/* do only once ,first one*/		

		/*generate my GO profile*/
		
		generate_GO_ssid(priv);

		//generate_GO_PSK(priv);
		strcpy(priv->p2pPtr->go_PSK,"12345678");	// for easy test

		
		/*channel list*/
		init_channel_list(priv);

	}

	if(priv->p2pPtr->rdyinit==0){			
		priv->p2pPtr->rdyinit=1;
	}



	
	P2P_show_status(priv,NULL);

	if(P2PMODE == P2P_DEVICE){	
		priv->p2pPtr->wait2listenState = 2; // enter listen mode by 1sec timer
	}
	
}



int p2pcmd_enable(struct rtl8192cd_priv *priv, unsigned char *data)
{

	if(P2PMODE == P2P_DEVICE){
		P2P_DEBUG("->\n");		
		P2P_listen(priv,NULL);
	}	
	return 0;
}	

int P2P_listen(struct rtl8192cd_priv* priv,unsigned char* data)
{
	
	/*check if listen channel is in social channel*/ 
	if(priv->pmib->p2p_mib.p2p_listen_channel !=1
		&& priv->pmib->p2p_mib.p2p_listen_channel!=6 
		&& priv->pmib->p2p_mib.p2p_listen_channel!=11)
	{
		P2P_DEBUG("invaild listen chn(%d) ; set chn to default =6\n",priv->pmib->p2p_mib.p2p_listen_channel);			
		priv->pmib->p2p_mib.p2p_listen_channel = 6;
	}

	P2P_DEBUG("listen;on chn(%d)\n",priv->pmib->p2p_mib.p2p_listen_channel);		
	stay_on_2G(priv);

	P2P_STATE = P2P_S_LISTEN;
	
	priv->pshare->CurrentChannelBW = HT_CHANNEL_WIDTH_20;
	SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
	
	SwChnl(priv, priv->pmib->p2p_mib.p2p_listen_channel, priv->pshare->offset_2nd_chan);
	
	// process receive probe_req frame

	// for assigned how long i will stay in listen state
	if(P2P_DISCOVERY){
		unsigned char randomX=0;
		get_random_bytes(&randomX , 1);
		randomX%=3;
		randomX+=1; // 1<= randomX <=3
		if (timer_pending(&priv->p2p_listen_timer_t))
			del_timer_sync(&priv->p2p_listen_timer_t);

		mod_timer(&priv->p2p_listen_timer_t, jiffies + RTL_MILISECONDS_TO_JIFFIES(randomX*100));				
	}
	
	return 0;
}

void P2P_listen_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	// now is on discovery procedure	; go to next state
	if(P2P_DISCOVERY && (P2P_STATE == P2P_S_LISTEN)){
		P2P_DISCOVERY +=1 ;
		P2P_PRINT("\n");
		P2P_DEBUG("find phase(%d)\n",P2P_DISCOVERY-1);
		P2P_search(priv,NULL);	
	}
	 
	return ;
}

/*P2P discovery related functions*/

void p2p_search_timer(unsigned long task_priv){

	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;	



	//P2P_DEBUG("\n");
	
	if (priv->ss_req_ongoing)
	{
		if (timer_pending(&priv->p2p_search_timer_t))
			del_timer_sync(&priv->p2p_search_timer_t);	

		//P2P_DEBUG("site survey is going...\n");		
		mod_timer(&priv->p2p_search_timer_t, jiffies + RTL_SECONDS_TO_JIFFIES(1));
	}else{

		int idx ;

		/*restore below two parameter*/
		if(priv->back_available_chnl_num){

			for(idx=0;idx<76;idx++)
				priv->available_chnl[idx]=priv->back_available_chnl[idx];
			
			priv->available_chnl_num = priv->back_available_chnl_num;		
			priv->back_available_chnl_num = 0 ;
				
		}

		
		// now is on discovery procedure	; (SCAN) is done go to next state (Listen)
		if(P2P_DISCOVERY && (P2P_STATE == P2P_S_SCAN)){			
				P2P_DEBUG("->\n");			
				P2P_listen(priv,NULL);
				return;
		}

		// now is on discovery procedure	; (SEARCH) is done go to next state (Listen)
		if(P2P_DISCOVERY && (P2P_STATE == P2P_S_SEARCH)){
			P2P_DEBUG("find phase run (%d) (search,end)\n",P2P_DISCOVERY-1)	;		
			if(P2P_DISCOVERY == 4)	{

				P2P_DISCOVERY = 0;

				p2p_show_ss_res(priv);
				
				P2P_DEBUG("P2P_discovery completed,back to ");
				
				if(P2PMODE==P2P_CLIENT){
					P2P_PRINT("client connected mode\n");
					P2P_STATE = P2P_S_CLIENT_CONNECTED_DHCPC_done;		
				}else{			
					P2P_PRINT("device listen\n");				
					P2P_DEBUG("->\n");					
					P2P_listen(priv,NULL);				
				}

			}else{
				P2P_DEBUG("->\n");			
				P2P_listen(priv,NULL);
			}
		}

		if((P2P_STATE == P2P_S_SEARCH) || (P2P_STATE == P2P_S_SCAN)){
				P2P_DEBUG("->\n");			
				P2P_listen(priv,NULL);
		}
		
	}
	
}

int P2P_scan(struct rtl8192cd_priv *priv, unsigned char *data)
{

	P2P_STATE = P2P_S_SCAN;
	priv->ss_ssidlen = 0;
	P2P_DEBUG("start_clnt_ss, ...\n");
	priv->ss_req_ongoing = 1;
	start_clnt_ss(priv);

	if (timer_pending(&priv->p2p_search_timer_t))
		del_timer_sync(&priv->p2p_search_timer_t);	
	
	mod_timer(&priv->p2p_search_timer_t, jiffies + RTL_SECONDS_TO_JIFFIES(2));
	
	return 0;
}


int P2P_search(struct rtl8192cd_priv *priv, unsigned char *data)
{


	int idx=0;

	P2P_STATE = P2P_S_SEARCH;

	/*copy below two parameter*/
	for(idx=0;idx<76;idx++)
		priv->back_available_chnl[idx]=priv->available_chnl[idx];

	priv->back_available_chnl_num = priv->available_chnl_num;
	
	/*copy below two parameter-end*/
	priv->available_chnl[0]=1;
	priv->available_chnl[1]=6;	
	priv->available_chnl[2]=11;	
	priv->available_chnl_num=3;



	priv->ss_ssidlen = 0;
	P2P_DEBUG("p2p search ...\n");
	priv->ss_req_ongoing = 1;
	start_clnt_ss(priv);


	if (timer_pending(&priv->p2p_search_timer_t))
		del_timer_sync(&priv->p2p_search_timer_t);
	
	mod_timer(&priv->p2p_search_timer_t, jiffies + RTL_SECONDS_TO_JIFFIES(2));		
	

	return 0;
}



 
int  P2P_show_status(struct rtl8192cd_priv *priv, unsigned char *data)
{
	int idx=0;
	if(!(OPMODE&WIFI_P2P_SUPPORT))
		printk("No under P2P mode\n");
	
	printk("OPMODE:	%02x\n",OPMODE);
	printk("P2PMODE: ");
	switch(P2PMODE)
	{
		case 	P2P_DEVICE:
			printk("P2P_DEVICE\n");
			break;
		case 	P2P_PRE_CLIENT:
			printk("P2P_PRE_CLIENT\n");
			break;
			
		case 	P2P_CLIENT:
			printk("P2P_CLIENT\n");		
			break;

		case 	P2P_PRE_GO:
			printk("P2P_PRE_GO\n");		
			break;

		case 	P2P_TMP_GO:
			printk("P2P_TMP_GO\n");
			break;
		default:
			printk("unknow type\n");

	}

	printk("P2P Status(%d):	",P2P_STATE);
	switch(P2P_STATE)
	{
		case 	P2P_S_IDLE:
			printk("IDLE\n");
			break;
		case 	P2P_S_SCAN:
			printk("SCAN\n");
			break;
			
		case 	P2P_S_LISTEN:
			printk("LISTEN\n");		
			break;

		case 	P2P_S_SEARCH:
			printk("SEARCH\n");		
			break;

		case 	P2P_S_PROVI_TX_REQ:
		case 	P2P_S_PROVI_WAIT_RSP:
		case 	P2P_S_PROVI_RX_RSP:
			printk("Provision procedure active\n");		
			break;			
		case 	P2P_S_PROVI_RX_REQ:
		case 	P2P_S_PROVI_TX_RSP:	
			printk("Provision procedure passive\n");		
			break;
		case 	P2P_S_NEGO_TX_REQ:
		case 	P2P_S_NEGO_WAIT_RSP:	
		case 	P2P_S_NEGO_TX_CONF:				
			printk("GO nego procedure active\n");		
			break;
		case 	P2P_S_NEGO_RX_REQ:
		case 	P2P_S_NEGO_TX_RSP:	
		case 	P2P_S_NEGO_WAIT_CONF:				
			printk("GO nego procedure passive\n");		
			break;
		case 	P2P_S_CLIENT_CONNECTED_DHCPC:
			printk("P2P client connected\n");
			break;			
		case 	P2P_S_CLIENT_CONNECTED_DHCPC_done:			
			printk("P2P client connected\n");
			break;

		case 	P2P_S_preGO2GO_DHCPD:
		case 	P2P_S_preGO2GO_DHCPD_done:			
			printk("P2P GO\n");
			break;
			
		default:
			printk("unknow type\n");

	}


	printk("p2p listen channel=\"%d\"\n",priv->pmib->p2p_mib.p2p_listen_channel);	
	printk("p2p operation channel=\"%d\"\n",priv->pmib->p2p_mib.p2p_op_channel);	
	printk("p2p intent value=\"%d\"\n",priv->pmib->p2p_mib.p2p_intent);			
	printk("p2p device name=\"%s\"\n",priv->pmib->p2p_mib.p2p_device_name);		
	printk("p2p pin code=\"%s\"\n",priv->pmib->p2p_mib.p2p_wsc_pin_code);	
	
	printk("GO info\n");
	printk("GO SSID=%s\n",priv->p2pPtr->my_GO_ssid);
	printk("GO PSK=%s\n",priv->p2pPtr->go_PSK);	

	if(priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G){
		printk("under PHY_BAND_2G\n");	
	}
	else if(priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G){
		printk("under PHY_BAND_5G\n");	
	}

	if(P2PMODE == P2P_TMP_GO){
		if(priv->p2pPtr->p2p_probe_rsp_ie_len)
			debug_out("go rsp IE",priv->p2pPtr->p2p_probe_rsp_ie ,priv->p2pPtr->p2p_probe_rsp_ie_len);


		for(idx=0;idx<MAX_P2P_CLIENT_MUN;idx++){
			if(priv->p2pPtr->assocPeers[idx].inuse){
				printk("%d) P2P client assoc2me:",idx);
				MAC_PRINT(priv->p2pPtr->assocPeers[idx].if_addr);
			}
		}
	
	}

	return 0;
}



/* display the result of p2p discovery*/
void p2p_show_ss_res(struct rtl8192cd_priv *priv)
{
	unsigned char* addrptr;
	int idx =0;
	//wait SiteSurvey completed

	P2P_PRINT("\n\n");
	P2P_DEBUG("result count=%d\n",priv->site_survey.count_backup);
	
	if(priv->ss_req_ongoing)	{
		P2P_PRINT("ss_req_ongoing\n");		
	}else{

		P2P_PRINT("index:");
		P2P_PRINT("address:		");		
		P2P_PRINT("role:	");	
		P2P_PRINT("channel:");			
		P2P_PRINT("wsc method:");				
		P2P_PRINT("name:	");
		P2P_PRINT("ssid(only go):\n");		
		for(idx=0;idx < priv->site_survey.count_backup; idx++){

				//if(strlen(priv->site_survey.bss_backup[idx].p2pdevname)==0)
				//	continue;
				
				P2P_PRINT("%d    ",idx);
				addrptr = priv->site_survey.bss_backup[idx].p2paddress;
				P2P_PRINT(";%02x%02x%02x:%02x%02x%02x	",addrptr[0],addrptr[1],addrptr[2],
					addrptr[3],addrptr[4],addrptr[5]);

				if(priv->site_survey.bss_backup[idx].p2prole == R_P2P_GO)
					P2P_PRINT(";GO    	");
				else if(priv->site_survey.bss_backup[idx].p2prole == R_P2P_DEVICE)
					P2P_PRINT(";DEVICE	");

				P2P_PRINT(";%d	",priv->site_survey.bss_backup[idx].channel);
				P2P_PRINT(";%02x		",priv->site_survey.bss_backup[idx].p2pwscconfig);				
				P2P_PRINT(";	%s	",priv->site_survey.bss_backup[idx].p2pdevname);
				if(priv->site_survey.bss_backup[idx].p2prole == R_P2P_GO)				
					P2P_PRINT(";%s	",priv->site_survey.bss_backup[idx].ssid);
				P2P_PRINT("\n");
				
		}
		P2P_PRINT("\n\n");
	}

}


 
int  P2P_show_command(struct rtl8192cd_priv *priv, unsigned char *data)
{


	P2P_PRINT("1)iwpriv wlan0 p2pcmd channel,6\n");
	P2P_PRINT("2)iwpriv wlan0 p2pcmd intent,6\n");
	P2P_PRINT("3)iwpriv wlan0 p2pcmd opchannel,6\n");
	P2P_PRINT("4)iwpriv wlan0 p2pcmd devname,rtl8196c-dev\n");	
	P2P_PRINT("1~4) need :iwpriv wlan0 p2pcmd apply\n");
	
	P2P_PRINT("6)iwpriv wlan0 p2pcmd scan\n");	
	P2P_PRINT("7)iwpriv wlan0 p2pcmd search\n");		
	P2P_PRINT("8)iwpriv wlan0 p2pcmd listen\n");	
	P2P_PRINT("9)iwpriv wlan0 p2pcmd find\n");	
	P2P_PRINT("10)iwpriv wlan0 p2pcmd discovery\n");		
	P2P_PRINT("\n");
	P2P_PRINT("11)iwpriv wlan0 p2pcmd asgo\n");		
	P2P_PRINT("12)iwpriv wlan0 p2pcmd bakdev\n");	
	P2P_PRINT("\n");
	P2P_PRINT("13)iwpriv wlan0 p2pcmd status\n");			

	return 0;
}



int p2pcmd_find(struct rtl8192cd_priv *priv, unsigned char *data)
{

	P2P_PRINT("start P2P find phase\n");	
	P2P_DISCOVERY = 1;
	priv->site_survey.count = 0;	
	priv->site_survey.count_backup	= 0;
	P2P_DEBUG("->\n");	
	P2P_listen(priv,NULL);	
	return 0;

}
int p2pcmd_discovery(struct rtl8192cd_priv *priv, unsigned char *data)
{

	P2P_PRINT("\n");	
	P2P_DEBUG("start P2P_discovery.....\n");	
	P2P_DISCOVERY = 1;
	priv->site_survey.count = 0;	
	priv->site_survey.count_backup	= 0;
	P2P_scan(priv,NULL);
	
	return 0;
}	

#define P2P_MODE_SWITCH	0	// no meaning

int p2p_as_preClient(struct rtl8192cd_priv *priv)
{

	//	unsigned long flags;
	//	P2P_DEBUG("\n");
	//SAVE_INT_AND_CLI(flags);
    //SMP_LOCK(flags);			
	
	rtl8192cd_close(priv->dev);
	OPMODE = (WIFI_P2P_SUPPORT | WIFI_STATION_STATE);
	P2PMODE = P2P_PRE_CLIENT;
	P2P_STATE = P2P_S_IDLE;

	/* clear SSID */
	memset(priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, '\0' , 32);
	priv->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = 0;

	// site suvery all
	priv->ss_ssidlen=0;

	rtl8192cd_open(priv->dev);


	//RESTORE_INT(flags);
	//SMP_UNLOCK(flags);	

	return 0;	
}

int p2p_as_GO(struct rtl8192cd_priv *priv, int GOtype){

	//unsigned long flags;
	//SAVE_INT_AND_CLI(flags);
    //SMP_LOCK(flags);
	
	rtl8192cd_close(priv->dev);
	OPMODE = (WIFI_P2P_SUPPORT | WIFI_AP_STATE);
	
	if(GOtype == P2P_PRE_GO)
		P2PMODE = P2P_PRE_GO;
	else if(GOtype == P2P_TMP_GO)
		P2PMODE = P2P_TMP_GO;

	P2P_DEBUG("MY GO SSID:%s\n",priv->p2pPtr->my_GO_ssid);
	P2P_DEBUG("MY PSK:%s\n",priv->p2pPtr->go_PSK);	
	/* set SSID */
	priv->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = priv->p2pPtr->my_GO_ssid_len;
	memset(priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, 0, 32);
	memcpy(priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, 
		priv->p2pPtr->my_GO_ssid, priv->p2pPtr->my_GO_ssid_len);



	/* set channel */
	//stay_on_2G(priv);	
	priv->pmib->dot11RFEntry.dot11channel = priv->pmib->p2p_mib.p2p_op_channel;

	/* set band */
	if(priv->pmib->p2p_mib.p2p_op_channel<=11)
		priv->pmib->dot11BssType.net_work_type=8+2; // G+N
	else	
		priv->pmib->dot11BssType.net_work_type=8+4; // A+N

	/*set security ;WPA2+AES*/
	priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm = _CCMP_PRIVACY_;			
	priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm = 2;			
	priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher = 8;
	priv->pmib->dot1180211AuthEntry.dot11EnablePSK = PSK_WPA2 ; // support WPA+WPA2
		
	/* PSK */
	strcpy(priv->pmib->dot1180211AuthEntry.dot11PassPhrase, priv->p2pPtr->go_PSK);		

	rtl8192cd_open(priv->dev);

	/*state change and indicate web server start udhcpd*/ 
	P2P_STATE = P2P_S_preGO2GO_DHCPD;

	//RESTORE_INT(flags);
	//SMP_UNLOCK(flags);
	return 0;	
}

int p2pcmd_force_GO(struct rtl8192cd_priv *priv, unsigned char *data)
{
	p2p_as_GO(priv,P2P_TMP_GO);
	// switch wscd mode to AP ;only change mode wps not start yet
	indicate_wscd(priv, MODE_AP_PROXY_REGISTRAR ,NULL , NULL);	
	return 0 ;
}	

int p2pcmd_backtoDev(struct rtl8192cd_priv *priv, unsigned char *data)
{
	unsigned long	flags;	

	P2P_DEBUG("\nReset to p2p device \n\n\n");

	if(P2PMODE == P2P_CLIENT){
		if(priv->p2pPtr->assocPeers[0].inuse 
			&& !is_zero_ether_addr(priv->p2pPtr->assocPeers[0].if_addr)){
			P2P_DEBUG("\n	issue disassoc to :");
			MAC_PRINT(priv->p2pPtr->assocPeers[0].if_addr);
			issue_disassoc(priv, priv->p2pPtr->assocPeers[0].if_addr , _RSON_UNSPECIFIED_ );				
		}
	}

	
	SAVE_INT_AND_CLI(flags);
	//SMP_LOCK(flags);
	rtl8192cd_close(priv->dev);
	OPMODE = (WIFI_P2P_SUPPORT | WIFI_STATION_STATE);
	P2PMODE = P2P_DEVICE;
	P2P_STATE = P2P_S_IDLE;

	
	/* set SSID */
	priv->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = 0;
	memset(priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, 0, 32);


	/* set channel */
	priv->pmib->dot11RFEntry.dot11channel = priv->pmib->p2p_mib.p2p_listen_channel;

	/* set band */
	//priv->pmib->dot11BssType.net_work_type=8+3; // B+G+N

	/*set security ;WPA2+AES*/
	priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm = _NO_PRIVACY_;			
	//priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm = 2;	
	//priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher = 8;

	/* PSK */
	memset(priv->pmib->dot1180211AuthEntry.dot11PassPhrase,0 ,65);		


	/*clean beacon P2P IE */ 
	priv->p2pPtr->p2p_beacon_ie_len = 0;

	/*make sure i am under 2G band*/
	stay_on_2G(priv);



	rtl8192cd_open(priv->dev);


	RESTORE_INT(flags);
	//SMP_UNLOCK(flags);

	// stop wscd	
	stop_wscd(priv,NULL);  

	// change back to enrollee mode	
	indicate_wscd(priv, MODE_CLIENT_UNCONFIG ,NULL , NULL);	

	//P2P_listen(priv, NULL); //0408 1sec timer will do it

	//==============			
	return 0;

}	


#define P2P_CONFIGURABILE_PARAMTER	0	// no meaning

int p2pcmd_set_listen_channel(struct rtl8192cd_priv *priv, unsigned char *data)
{
	int dwekk_chn=_atoi(data , 10);
	
	if(dwekk_chn !=1 &&  dwekk_chn!=6 &&  dwekk_chn!=11){
		P2P_DEBUG("invaild chn(%d) ; set chn to default =6\n",dwekk_chn);			
		priv->pmib->p2p_mib.p2p_listen_channel = 6 ;
	}else{
		priv->pmib->p2p_mib.p2p_listen_channel = dwekk_chn ;
		P2P_DEBUG("set listen_channel to (%d) OK!!\n", priv->pmib->p2p_mib.p2p_listen_channel);
	}		
	return 0;

}

int p2pcmd_set_intent_value(struct rtl8192cd_priv *priv, unsigned char *data)
{
	int intentVal = _atoi(data , 10);	
	if(intentVal>15 ||intentVal<0 ){
		P2P_DEBUG("invaild intent value\n");
	}else{
		priv->pmib->p2p_mib.p2p_intent = intentVal ;
		P2P_DEBUG("set intentVal to (%d) OK!!\n",priv->pmib->p2p_mib.p2p_intent);			
	}	
	return 0;
}


int p2pcmd_set_op_channel(struct rtl8192cd_priv *priv, unsigned char *data)
{
	int opchannel = _atoi(data , 10);
	priv->pmib->p2p_mib.p2p_op_channel = opchannel ;
	P2P_DEBUG("set op channel to (%d) OK!!\n",priv->pmib->p2p_mib.p2p_op_channel);		
	return 0;
}


int p2pcmd_set_devicename(struct rtl8192cd_priv *priv, unsigned char *data)
{
	unsigned char devname[33];
	devname[32]='\0';
	strcpy(devname, data);
	P2P_DEBUG("set dev name to %s \n",devname);
	strcpy(priv->pmib->p2p_mib.p2p_device_name ,devname );
	return 0;

}

int p2pcmd_apply(struct rtl8192cd_priv *priv, unsigned char *data)
{
	p2p_init(priv);		
	return 0;
}


#define P2P_COMMAND_LIST	0	// no meaning


struct p2p_cmd_list p2p_cmd_tbl_lev1[] = {

	{"channel",p2pcmd_set_listen_channel},
	{"intent",p2pcmd_set_intent_value},
	{"opchannel",p2pcmd_set_op_channel},
	{"devname",p2pcmd_set_devicename},	
	{"asgo",p2pcmd_force_GO},	
	{"bakdev",p2pcmd_backtoDev},	
	{"apply",p2pcmd_apply},		
	{"enable",p2pcmd_enable},
	{"scan",P2P_scan},			
	{"listen",P2P_listen},	
	{"search",P2P_search},				
	{"discovery",p2pcmd_discovery},			
	{"find",p2pcmd_find},					
	{"status",P2P_show_status},
	{"help",P2P_show_command}

};

int p2pcmdCnt = sizeof(p2p_cmd_tbl_lev1) / sizeof(struct p2p_cmd_list);
int process_p2p_cmd(struct rtl8192cd_priv *priv, unsigned char *data)
{

	char *val=NULL;	
	int ix;
	int res=0;
	int found=0;
	
	for(ix=0; ix<p2pcmdCnt; ix++) {
		val = p2p_get_token((char *)data, p2p_cmd_tbl_lev1[ix].cmd);
		if (val) {
			res = p2p_cmd_tbl_lev1[ix].p2p_cmd_func(priv , val) ;
			found = 1;
			break;
		}
	}
	if(found==0){
		P2P_PRINT("usage:	\n");		
		for(ix=0; ix<p2pcmdCnt; ix++)
			P2P_PRINT("%s \n", p2p_cmd_tbl_lev1[ix].cmd	);
	}
	return res;

}



