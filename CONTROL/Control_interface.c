#include "Control_interface.h"
#include "control_app.h"
#include <string.h>
#include "gpio.h"
#include "usart.h"
//#include "flash.h"
#include "bt_app.h"
#include "IoT_Hub.h"
#include "Control_app.h"

battery_info_struct curr_bat;

control_struct controller={
	{0,0,LOW_SPEED,HIGH,HUNHE,VOT48V,XF_INVALID},
	{0,0,LOW_SPEED,HIGH,HUNHE,VOT48V,XF_INVALID}};

extern uint8_t get_electric_gate_status(void);
extern void bt_name_modify(char* name);

void control_send_data(uint8_t* data, uint16_t len)
{
	
}

/*****************************************************************************
 * FUNCTION
 *  zt_controller_send
 * DESCRIPTION
 * ����������ӿ�
 * PARAMETERS
 * uint8_t addr  ��ַ
 * cmd cmd_control ��������
 *        cmd_read  ��ȡ����
 *  data 1:����2:��Ƿѹֵ3:�����л�4:�����޸�5:��Դ��ѹ
 *  data 2: ֵ
 * RETURNS
 *  void
 *****************************************************************************/
void zt_controller_send(uint8_t addr,cmd_enum cmd, uint8_t data1,uint8_t data2)
{
	uint16_t check_sum = 0;
	uint8_t i, send_data[] = {0x3a,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0x0a};

	send_data[1] = addr;
	send_data[2] = cmd;
	send_data[3] = 0x02;	//����
	send_data[4] = data1;
	send_data[5] = data2;

	for(i=0; i<5;i++)
		check_sum += send_data[i+1];
	
	send_data[6] =check_sum&0xff;
	send_data[7] =check_sum>>8;

	printf("uart2 send addr=%x,cmd=%x,data1=%x,data2=%x",addr,cmd,data1,data2);
	control_send_data(send_data, sizeof(send_data));
}

bool control_proc(uint8_t* buf, uint16_t len)
{
	uint16_t i,checksum=0;

	for(i=0; i<len-2; i++)
	{
		checksum += buf[i];
	}
	printf("checksum=%x\n",checksum);
	if(!((checksum&0x00ff)==(uint16_t)buf[len-2] && (checksum&0xff00)>>8 == (uint16_t)buf[len-1]))
		return false;
	printf("checksum ok\n");

	if(buf[0]==ADDR_CONTROL && get_electric_gate_status())	//��������ַ
	{
		if(buf[1]==0x01)	//�����������Ӧ
		{
			printf("Recv Control RSP len = %d\n",len);
			switch(buf[3])
			{
				case 0x01:	//����
					if(buf[4])
						printf("Speed change Success\n");
					else
						printf("Speed change Fail\n");
					break;
				case 0x02:	//��Ƿѹֵ
					if(buf[4])
						printf("Qianya Success\n");
					else
						printf("Qianya Fail\n");
					break;
				case 0x03:	//�����л�
					if(buf[4])
						printf("Zhuli Success\n");
					else
						printf("Zhuli Fail\n");
					break;
				case 0x04:	//�����޸�
					if(buf[4])
						printf("Guzhang xiufu Success\n");
					else
						printf("Guzhang xiufu Fail\n");
					break;
				default:
					break;
			}
		}
		else if(buf[1]==0x02)	//��ȡ����
		{
			controller.actual.fault = buf[3];
			controller.actual.hall = buf[4]+buf[5]<<8+buf[6]<<16+buf[7]<<24;
			controller.actual.tiaosu = buf[8];
			controller.actual.qianya = buf[9];
			controller.actual.zhuli = buf[10];
			controller.actual.dy = buf[11];
			controller.actual.xf = buf[12];
			printf("Recv Upload tiaosu=%d,qy=%d,zhuli=%d,dianyuan=%d,xiufu=%d",controller.actual.tiaosu,controller.actual.qianya,
				controller.actual.zhuli,controller.actual.dy,controller.actual.xf);
		}
	}
	else if(buf[0]==ADDR_BAT)
	{
		uint16_t data=buf[3]+buf[4]*0x100;
		switch(buf[1])
		{
			case bat_temp:
				curr_bat.temp = data;
				break;
			case bat_vol:
				curr_bat.voltage = data;
				printf("voltage=%d",data);
				break;
			case bat_curr:
				curr_bat.current = data;
				break;
			case bat_cap:
				curr_bat.residual_cap = data;
				break;
			case bat_total_cap:
				curr_bat.total_cap = data;
				break;
			case bat_cycle:
				curr_bat.cycle_count = data;
				break;
			case bat_interval:
				curr_bat.interval = data;
				break;
			case bat_max_interval:
				curr_bat.max_interval = data;
				break;
		}
	}
	
	return true;
}

void get_int_num(uint8_t*buf,uint8_t*ip1,uint8_t*ip2,uint8_t*ip3,uint8_t*ip4)
{
	uint8_t *head,*tail,tmp[4];
	
	head = buf;
	tail = (uint8_t*)strstr(head,".");
	memset(tmp,0,sizeof(tmp));
	memcpy(tmp,head,tail-head);
	*ip1 = atoi(tmp);

	head = tail+1;
	tail = (uint8_t*)strstr(head,".");
	memset(tmp,0,sizeof(tmp));
	memcpy(tmp,head,tail-head);
	*ip2 = atoi(tmp);

	head = tail+1;
	tail = (uint8_t*)strstr(head,".");
	memset(tmp,0,sizeof(tmp));	
	memcpy(tmp,head,tail-head);
	*ip3 = atoi(tmp);

	head = tail+1;
	tail = (uint8_t*)strstr(head,",");
	memset(tmp,0,sizeof(tmp));	
	memcpy(tmp,head,tail-head);
	*ip4 = atoi(tmp);
}

void modify_service_address(uint8_t* buf)
{
	//type:2,domain:www.liabar.com,port:9000#
	//type:1,ip:139.224.3.220,port:9000#
/*	network_info_struct network={0};
	uint8_t tmp[64]={0};
	uint8_t *head=NULL,*tail=NULL;
	
	head = (uint8_t*)strstr(buf,"type:");
	tail = (uint8_t*)strstr(buf,"#");
	if(head&&tail)
	{
		printf("%s",buf);
		head += strlen("type:");
		tail = (uint8_t*)strstr(head,",");
		memcpy(tmp, head,tail-head);
		network.ym_type = atoi(tmp);
		printf("network.ym_type=%d",network.ym_type);
		if(network.ym_type==2)
		{
			head = (uint8_t*)strstr(buf,"domain:");
			if(head)
			{
				head+= strlen("domain:");
				tail = (uint8_t*)strstr(head,",");
				memcpy(network.domain,head,tail-head);

				head = (uint8_t*)strstr(buf,"port:");
				head+=strlen("port:");
				tail = (uint8_t*)strstr(head,"#");
				memset(tmp,0,sizeof(tmp));
				memcpy(tmp,head,tail-head);
				network.port = atoi(tmp);

				sprintf(tmp,"OK type:%d,%s,%d\r\n",network.ym_type,network.domain,network.port);
				control_send_data(tmp,strlen(tmp));
				zt_write_config_in_fs(NETWORK_FILE, (uint8_t*)&network,  sizeof(network_info_struct));
				StartTimer(GetTimerID(ZT_DELAY_RESTART_TIMER),1000,restartSystem);
			}
		}
		else if(network.ym_type==1)
		{
			head = (uint8_t*)strstr(buf,"ip:");
			if(head)
			{
				head+= strlen("ip:");
				tail = (uint8_t*)strstr(head,",")+1;
				memcpy(tmp, head, tail-head);
				get_int_num(tmp,&network.ip[0],&network.ip[1],&network.ip[2],&network.ip[3]);
							
				head = (uint8_t*)strstr(buf,"port:");
				head+=strlen("port:");
				tail = (uint8_t*)strstr(head,"#");
				memset(tmp,0,sizeof(tmp));
				memcpy(tmp,head,tail-head);
				network.port = atoi(tmp);

				sprintf(tmp,"OK type:%d,%d:%d:%d:%d,%d",network.ym_type,network.ip[0],network.ip[1],network.ip[2],network.ip[3],network.port);
				control_send_data(tmp,strlen(tmp));
				printf("%s",tmp);
				zt_write_config_in_fs(NETWORK_FILE, (uint8_t*)&network,  sizeof(network_info_struct));
				StartTimer(GetTimerID(ZT_DELAY_RESTART_TIMER),1000,restartSystem);
			}
		}
	}*/
}

bool parse_control_cmd(uint8_t* buf, uint16_t len)
{
	uint16_t i,len2;
	uint8_t*head=NULL,*tail=NULL;
	uint8_t req[64]={0};
	bool flag = false;

	for(i=0; i<len; i++)
	{
		if(buf[i]==0x3a)
		{
			head = buf+i;
		}
		else if(buf[i]==0x0d&&buf[i+1]==0x0a)
		{
			tail = buf+i;
			if(head)
			{
				len2 = tail-(head+1);
				memset(req,0,sizeof(req));
				memcpy(req, head+1, len2);
				control_proc(req,len2);
				flag = true;
			}
		}
	}

	if(!flag)
	{
		if(head = (char*)strstr(buf,"imei:"))
		{
			head += strlen("imei:");
			tail = (char*)strstr(head,"#");
			if(head&&tail)
			{
				memset(g_flash.imei,0,sizeof(g_flash.imei));
				memcpy(g_flash.imei,head,tail-head);
				write_flash(CONFIG_ADDR, (uint16_t*)&g_flash,(uint16_t)sizeof(flash_struct)/2);
				printf("write imei=%sOK\r\n",g_flash.imei);
				flag = true;
				reset_system();
			}
		}
		else if(head = (char*)strstr(buf,"name:"))
		{
		
			head += strlen("name:");
			tail = (char*)strstr(head,"#");
			if(head&&tail)
			{
				char bt_name[16]={0};
				char buf[64]={0};
				memcpy(bt_name,head,tail-head);
				bt_name_modify(bt_name);
				flag = true;
			//	reset_system();
			}
		}
	}
	return flag;
//	modify_service_address(buf);
	
}

