/*****************************************************************************
**
**  Name:           bluetooth_interface.c
**
**  Description:    Bluetooth Manager application
**
**  Copyright (c) 2010-2014, Broadcom Corp., All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#include "bsa_api.h"

#include "app_xml_utils.h"

#include "app_disc.h"
#include "app_utils.h"
#include "app_dm.h"

#include "app_services.h"
#include "app_mgt.h"

#include "app_manager.h"
#include "app_avk.h"
#include "app_hs.h"
#include "bluetooth_interface.h"

#define MAX_PATH_LEN  256

/*
 * Extern variables
 */
extern tAPP_MGR_CB app_mgr_cb;
extern tAPP_XML_CONFIG         app_xml_config;
extern tAPP_AVK_CB app_avk_cb;

/*
 * Extern funtion
 * */
extern void store_connected_dev(BD_ADDR bt_mac_addr);
extern void bt_event_transact(void *p, APP_BT_EVENT event, void *reply, int *len);
extern void read_connected_dev(BD_ADDR bt_mac_addr);

/*
 * global variables
 */
tAPP_DISCOVERY_CB app_discovery_cb;
char bta_conf_path[MAX_PATH_LEN] = {0};
int connect_link_status = 0;

/*static variables */
static tBSA_SEC_IO_CAP g_sp_caps = 0;
static BD_ADDR     cur_connected_dev;        /* BdAddr of connected device */
static BD_ADDR     last_connected_dev;       /* store connected dev last reboot */
static void *p_cbt = NULL;
static int bluetooth_on = 0;
static int discoverable;
static int connectable;

//link status
static int link_status = 0;

//avk status
static int avk_music_playing = 0;
static int avk_start_status = 0;
static int avk_disconnect_cmd = 0;
static int avk_connected_inner = 0;
static struct timeval avk_connected_time1;
static struct timeval avk_connected_time2;

//discovery
static int  disc_flag = 0;
static int  dev_nums = 0;
static char dev_info[4096] = {0};

//avk element attr
static int get_element_flag = 0;
static s_avk_element_attr_t s_avk_element_attr;

//tAvkCallback
static void app_disc_callback(tBSA_DISC_EVT event, tBSA_DISC_MSG *p_data);
static void app_avk_callback(tBSA_AVK_EVT event, tBSA_AVK_MSG *p_data);
#if 0
static void app_hs_callback(tBSA_HS_EVT event, tBSA_HS_MSG *p_data);
#endif

static int store_disc_results(char *buf, int *buf_len)
{
    int num = 0, index = 0;
    char *pw = NULL;
    int  len = 0;

    pw = buf;
    len = 0;
    for (index = 0; index < APP_DISC_NB_DEVICES; index++)
    {
        if (app_discovery_cb.devs[index].in_use != FALSE)
        {
		  num++;
		  len=len+4;

		  sprintf(pw, "Dev:%d", num);
		  pw=pw+4;
		  if(num<10){
		      len = len+1;
		      pw=pw+1;
		  }else if(num<100){
		      len=len+2;
		      pw=pw+2;
		  }else{
		      goto end;
		  }

		  sprintf(pw, "\tBdaddr:%02x:%02x:%02x:%02x:%02x:%02x",
                    app_discovery_cb.devs[index].device.bd_addr[0],
                    app_discovery_cb.devs[index].device.bd_addr[1],
                    app_discovery_cb.devs[index].device.bd_addr[2],
                    app_discovery_cb.devs[index].device.bd_addr[3],
                    app_discovery_cb.devs[index].device.bd_addr[4],
                    app_discovery_cb.devs[index].device.bd_addr[5]);
            len = len+25;
            pw=pw+25;

            sprintf(pw, "\tName:%s", app_discovery_cb.devs[index].device.name);
            len=len+6;
            len=len+strlen((const char *)app_discovery_cb.devs[index].device.name);
            pw=pw+6;
            pw=pw+strlen((const char *)app_discovery_cb.devs[index].device.name);

            *pw='\n';
            len=len+1;
            pw=pw+1;
        }
    }

end:
	  *pw='\0';
	  *buf_len = len;
    if(num > 0){
        *(pw-1)='\0';
        *buf_len = (len-1);
    }

    return	num;
}

static void app_disc_callback(tBSA_DISC_EVT event, tBSA_DISC_MSG *p_data)
{
    int len = 4096;

    switch(event)
    {
	case BSA_DISC_CMPL_EVT: /* Discovery complete. */
        dev_nums = store_disc_results(dev_info, &len);
        //bt_event_transact(p_sbt, APP_MGR_DISC_RESULTS, buf, &len);
        disc_flag = 1;
        break;

        default:
        ;
    }
}

static void app_avk_callback(tBSA_AVK_EVT event, tBSA_AVK_MSG *p_data)
{

	  switch(event)
	  {
		  case BSA_AVK_OPEN_EVT:
		  {
			  /* open status must be success */
			  if (p_data->open.status == BSA_SUCCESS){
			      APP_DEBUG0("avk connected!\n");
			      bdcpy(cur_connected_dev, p_data->open.bd_addr);
			      store_connected_dev(cur_connected_dev);
                  avk_connected_inner = 1;
                  bt_event_transact(p_cbt, APP_AVK_CONNECTED_EVT, NULL, NULL);
			  }

		      break;
		  }
		  case BSA_AVK_CLOSE_EVT:
		  {
		      APP_DEBUG0("avk disconnected!\n");
              memset(cur_connected_dev, 0, sizeof(cur_connected_dev));
              avk_connected_inner = 0;
		      break;
		  }
	      case BSA_AVK_START_EVT:
	      {
              /* already received BSA_AVK_OPEN_EVT */
              if(avk_connected_inner == 1){
                  avk_connected_inner = 2;
                  gettimeofday(&avk_connected_time1, NULL);
                  bt_event_transact(p_cbt, APP_AVK_CONNECT_COMPLETED_EVT, NULL, NULL);
              }

              /* start music playing */
		  if(p_data->start.streaming == TRUE)
		  {
                  APP_DEBUG0("BT is playing music!\n");
                  avk_music_playing = 1;
                  bt_event_transact(p_cbt, APP_AVK_START_EVT, NULL, NULL);
		  }

			break;
		}

		case BSA_AVK_STOP_EVT:
		{
			APP_DEBUG0("BT is stop music!\n");
			avk_music_playing = 0;
			bt_event_transact(p_cbt, APP_AVK_STOP_EVT, NULL, NULL);
			break;
		}

		case BSA_AVK_GET_ELEM_ATTR_EVT:
		{
			int i = 0;
			tBSA_AVK_STRING *p_name = NULL;

			APP_DEBUG0("AVK_GET_ELEM_ATTR_EVT cback");

			memset(&s_avk_element_attr, 0, sizeof(s_avk_element_attr));
			for (i = 0; i < p_data->elem_attr.num_attr; i++){
				p_name = &(p_data->elem_attr.attr_entry[i].name);
				switch(p_data->elem_attr.attr_entry[i].attr_id)
				{
					case BSA_AVRC_MEDIA_ATTR_ID_TITLE:
						{
							memcpy(s_avk_element_attr.title, p_name->data, p_name->str_len);
							s_avk_element_attr.title[p_name->str_len] = '\0';
							break;
						}

					case BSA_AVRC_MEDIA_ATTR_ID_ARTIST:
						{
							memcpy(s_avk_element_attr.artist, p_name->data, p_name->str_len);
							s_avk_element_attr.artist[p_name->str_len] = '\0';
							break;
						}

					case BSA_AVRC_MEDIA_ATTR_ID_ALBUM:
						{
							memcpy(s_avk_element_attr.album, p_name->data, p_name->str_len);
							s_avk_element_attr.album[p_name->str_len] = '\0';
							break;
						}

					case BSA_AVRC_MEDIA_ATTR_ID_PLAYING_TIME:
						{
							memcpy(s_avk_element_attr.playing_time, p_name->data, p_name->str_len);
							s_avk_element_attr.playing_time[p_name->str_len] = '\0';
							break;
						}

					default:
						;
				}
			}
			get_element_flag = 1;

			break;
		}

	      default:
	          ;
	  }
}

#if 1
static void app_hs_callback(tBSA_HS_EVT event, tBSA_HS_MSG *p_data)
{
    UINT16 handle = 0;
    tBSA_HS_CONN_CB *p_conn;

    /* retrieve the handle of the connection for which this event */
    handle = p_data->hdr.handle;

    /* retrieve the connection for this handle */
    p_conn = app_hs_get_conn_by_handle_external(handle);

    switch(event)
    {
        case BSA_HS_CONN_EVT:
        {
            APP_DEBUG0("hs connected!\n");

            /* check if this conneciton is already opened */
            if (p_conn->connection_active)
            {
                printf("BSA_HS_CONN_EVT: connection already opened for handle %d\n", handle);
                break;
            }

            bt_event_transact(p_cbt, APP_HS_CONNECTED_EVT, NULL, NULL);
            break;
        }
        case BSA_HS_CLOSE_EVT:
        {
		  APP_DEBUG0("hs disconnected!\n");
		  bt_event_transact(p_cbt, APP_HS_DISCONNECTED_EVT, NULL, NULL);
		  break;
        }
        case BSA_HS_AUDIO_OPEN_EVT:
        {
            APP_DEBUG0("hs audio open!\n");
            break;
        }

		case BSA_HS_RING_EVT:
		{
			APP_DEBUG0("hs ring event!\n");
			bt_event_transact(p_cbt, APP_HS_RING_EVT, NULL, NULL);
			break;
		}

        default:
            ;
    }
}
#endif

/*******************************************************************************
 **
 ** Function         app_mgr_mgt_callback
 **
 ** Description      This callback function is called in case of server
 **                  disconnection (e.g. server crashes)
 **
 ** Parameters
 **
 ** Returns          void
 **
 *******************************************************************************/
static BOOLEAN app_mgr_mgt_callback(tBSA_MGT_EVT event, tBSA_MGT_MSG *p_data)
{
    switch(event)
    {
    case BSA_MGT_STATUS_EVT:
        APP_DEBUG0("BSA_MGT_STATUS_EVT");
        if (p_data->status.enable)
        {
            APP_DEBUG0("Bluetooth restarted => re-initialize the application");
            app_mgr_config();
        }
        break;

    case BSA_MGT_DISCONNECT_EVT:
        APP_DEBUG1("BSA_MGT_DISCONNECT_EVT reason:%d", p_data->disconnect.reason);
        break;

    default:
        break;
    }
    return FALSE;
}

static void bsa_sec_callback(tBSA_SEC_EVT event, tBSA_SEC_MSG *p_data)
{
    int link_reason = 0;
    int reply_len = 0;

    switch(event){
        case BSA_SEC_LINK_UP_EVT:
            APP_DEBUG0("BSA_SEC_LINK_UP_EVT");
            link_status = 1;
            break;

        case BSA_SEC_LINK_DOWN_EVT:
            APP_DEBUG1("BSA_SEC_LINK_DOWN_EVT, avk disconnect cmd %d avk connected %d", avk_disconnect_cmd, avk_connected_inner);
            if(avk_disconnect_cmd == 1 || avk_connected_inner == 0){
                avk_disconnect_cmd = 0;
                link_status = 0;
				connect_link_status = 0;
                link_reason = p_data->link_down.status;
                reply_len = 4;
                bt_event_transact(p_cbt, APP_AVK_DISCONNECTED_EVT, (void *)&link_reason, &reply_len);
                APP_DEBUG0("BSA_SEC_LINK_DOWN_EVT return from app!\n");
            }
            break;

        default:
            break;
    }
}

int bluetooth_init()
{
    discoverable = 1;
    connectable = 1;
    app_mgr_config_init();
	return 0;
}

int bluetooth_start(void *p, char *p_conf)
{
    int mode;

    p_cbt = p;

    if(p_conf && p_conf[0] != '\0')
    {
	printf("Bt conf file %s\n", p_conf);
        strncpy(bta_conf_path, p_conf, MAX_PATH_LEN-1);
    }

    /* Init App manager */
    app_mgt_init();
    if (app_mgt_open(NULL, app_mgr_mgt_callback) < 0)
    {
        APP_ERROR0("Unable to connect to server");
        return -1;
    }

    /* set bsa sec callback */
    app_mgr_sec_set_callback(bsa_sec_callback);

    /* Init XML state machine */
    app_xml_init();

    if (app_mgr_config())
    {
        APP_ERROR0("Couldn't configure successfully, exiting");
        return -1;
    }
    g_sp_caps = app_xml_config.io_cap;

    /* Display FW versions */
    app_mgr_read_version();

    /* Get the current Stack mode */
    mode = app_dm_get_dual_stack_mode();
    if (mode < 0)
    {
        APP_ERROR0("app_dm_get_dual_stack_mode failed");
        return -1;
    }
    else
    {
        /* Save the current DualStack mode */
        app_mgr_cb.dual_stack_mode = mode;
        APP_INFO1("Current DualStack mode:%s", app_mgr_get_dual_stack_mode_desc());
    }

    bluetooth_on = 1;

    return 0;
}

void start_app_avk()
{
    /* Init avk Application */
    app_avk_init(app_avk_callback);
    //auto register
    app_avk_register();

    avk_start_status = 1;
}

void start_app_avk_no_avrcp()
{
    /* Init avk Application */
    app_avk_init_no_avrcp(app_avk_callback);
    //auto register
    app_avk_register();

    avk_start_status = 1;
}

void start_app_hs()
{
    /* Init Headset Application */
	app_hs_init();
    /* Start Headset service*/
	app_hs_start(app_hs_callback);
}

void s_set_bt_name(const char *name)
{
    if(bluetooth_on == 1){
        app_mgr_set_bd_name(name);
    }else{
        app_mgr_config_bd_name(name);
    }
}


void s_set_discoverable(int enable)
{
    if(((enable!=0)&&(discoverable==1))
        || ((enable==0)&&(discoverable==0))){
        printf("discoverable cur %d, to %d\n", discoverable, enable);
        return;
    }

    if(enable){
        discoverable=1;
    }else{
        discoverable=0;
    }
    if(bluetooth_on == 1){
        app_dm_set_visibility(discoverable, connectable);
    }else{
        app_mgr_config_discoverable(discoverable);
    }
}

void s_set_connectable(int enable)
{
    if(((enable!=0)&&(connectable==1))
        ||((enable==0)&&(connectable==0))){
        printf("connectable cur %d, to %d\n", connectable,enable);
        return ;
    }

    if(enable){
        connectable=1;
    }else{
        connectable=0;
    }

    if(bluetooth_on == 1){
    app_dm_set_visibility(discoverable, connectable);
    }else{
        app_mgr_config_connectable(connectable);
    }
}

void s_start_discovery(int time)
{
    disc_flag = 0;
    dev_nums = 0;
    app_disc_start_regular(app_disc_callback);
}

int s_get_disc_results(char *disc_results, int *len)
{
    int times = 0;

    while((disc_flag == 0) && (times < 300)){
        usleep(100*1000);
        times++;
    }

    if(disc_flag == 0 || dev_nums == 0){
        disc_results[0] = '\0';
        *len = 0;
	return 0;
    }else{
	strncpy(disc_results, dev_info, *len);
	*len = strlen(dev_info);
	disc_results[*len] = '\0';
	return dev_nums;
    }
}

void s_set_volume(int volume)
{
    return ;
}

void s_set_volume_up()
{
    tAPP_AVK_CONNECTION *connection = NULL;

    connection = app_avk_find_connection_by_bd_addr(cur_connected_dev);
    if(connection)
    {
        printf("set_volume_up app_avk volume %d\n", app_avk_cb.volume);
        app_avk_cb.volume += 8;
        if(app_avk_cb.volume > 127){
            app_avk_cb.volume = 127;
        }
        app_avk_reg_notfn_rsp(app_avk_cb.volume,
            connection->rc_handle,
            connection->volChangeLabel,
            AVRC_EVT_VOLUME_CHANGE,
            BTA_AV_RSP_CHANGED);
        printf("volume up change locally at TG\n");
    }
    else
    {
        printf("Connection is NULL when set volume up\n");
    }
}

void s_set_volume_down()
{
    tAPP_AVK_CONNECTION *connection = NULL;

    connection = app_avk_find_connection_by_bd_addr(cur_connected_dev);
    if(connection)
    {
	  printf("set_volume_down app_avk volume %d\n", app_avk_cb.volume);
        if(app_avk_cb.volume < 8 ){
            app_avk_cb.volume = 0;
        } else {
            app_avk_cb.volume -= 8;
        }

        app_avk_reg_notfn_rsp(app_avk_cb.volume,
            connection->rc_handle,
            connection->volChangeLabel,
            AVRC_EVT_VOLUME_CHANGE,
            BTA_AV_RSP_CHANGED);
        printf("volume down change locally at TG\n");
    }
    else
    {
        printf("Connection is NULL when set volume down\n");
    }
}

int s_connect_auto()
{
    int i = 0;

    printf("s connect auto\n");

    /* wait disconnect completed */
    while(link_status == 1){
        usleep(100*1000);
        i++;
        if(i>60){
            break;
        }
    }
    printf("link status %d\n", link_status);

	connect_link_status = 1;
    memset(last_connected_dev, 0, sizeof(last_connected_dev));
    read_connected_dev(last_connected_dev);
    return app_avk_auto_connect(last_connected_dev);
}

int s_connect_dev_by_addr(S_BT_ADDR s_bt_addr)
{
    int i = 0;
    BD_ADDR bd_addr;

    for(i = 0; i < BD_ADDR_LEN; i++){
        bd_addr[i] = s_bt_addr[i];
    }
	connect_link_status = 1;

    app_avk_connect_by_addr(bd_addr);
    return 0;
}

void s_disconnect()
{
    int i, time_val;
    printf("s disconnect, avk connected %d, music playing %d\n", avk_connected_inner, avk_music_playing);

    /* free audio device */
    app_avk_close_pcm_alsa();

    i=0;
    while(avk_connected_inner != 2){
           usleep(100*1000);
           i++;
           if(i>60){
               break;
           }
    }
    printf("avk connected %d, %d s\n", avk_connected_inner, (i/10));

    gettimeofday(&avk_connected_time2, NULL);
    time_val = avk_connected_time2.tv_sec - avk_connected_time1.tv_sec;
    if((time_val >= 0) && (time_val < 5)){
        time_val = 5-time_val;
        usleep(time_val * 1000 * 1000);
    }

    avk_disconnect_cmd = 1;

    app_avk_close(cur_connected_dev);

#if 0
    stop_app_avk();
    usleep(500*1000);

    /* start avk app */
    start_app_avk();
#endif
}

void s_avk_play()
{
    tAPP_AVK_CONNECTION *connection = NULL;

    printf("a avk play\n");

	  connection = app_avk_find_connection_by_bd_addr(cur_connected_dev);
	  if(connection)
	  {
        app_avk_play_start(connection->rc_handle);
    }
    else
    {
	  printf("Connection is NULL when playing\n");
    }
}

void s_avk_pause()
{
	  tAPP_AVK_CONNECTION *connection = NULL;

          printf("s avk pause\n");

	  connection = app_avk_find_connection_by_bd_addr(cur_connected_dev);
	  if(connection)
	  {
        app_avk_play_pause(connection->rc_handle);
    }
    else
    {
	  printf("Connection is NULL when pausing\n");
    }
}

void s_avk_stop()
{
    tAPP_AVK_CONNECTION *connection = NULL;

    printf("s avk stop\n");
    connection = app_avk_find_connection_by_bd_addr(cur_connected_dev);
    if(connection)
    {
        app_avk_play_stop(connection->rc_handle);
    }
    else
    {
        printf("Connection is NULL when pausing\n");
    }
}

void s_avk_close_pcm_alsa()
{
    printf("s avk close pcm alsa\n");
    app_avk_close_pcm_alsa();
}

void s_avk_resume_pcm_alsa()
{
    printf("s avk resume pcm alsa\n");
    app_avk_resume_pcm_alsa();
}

void s_avk_play_previous()
{
	  tAPP_AVK_CONNECTION *connection = NULL;

	  connection = app_avk_find_connection_by_bd_addr(cur_connected_dev);
	  if(connection)
	  {
        app_avk_play_previous_track(connection->rc_handle);
    }
    else
    {
	  printf("Connection is NULL when playing pre\n");
    }
}

void s_avk_play_next()
{
    tAPP_AVK_CONNECTION *connection = NULL;

	  connection = app_avk_find_connection_by_bd_addr(cur_connected_dev);
	  if(connection)
	  {
        app_avk_play_next_track(connection->rc_handle);
    }
    else
    {
	  printf("Connection is NULL when playing next\n");
    }
}

int s_avk_get_element_attr(s_avk_element_attr_t *p_s_avk_element_attr)
{
	int times = 0;
	tAPP_AVK_CONNECTION *connection = NULL;

	if (!p_s_avk_element_attr){
		printf("Error: s_avk_get element attr param error!\n");
		return -1;
	}

	get_element_flag = 0;
	memset(p_s_avk_element_attr, 0, sizeof(s_avk_element_attr));
	connection = app_avk_find_connection_by_bd_addr(cur_connected_dev);
	if(connection)
	{
		app_avk_rc_get_element_attr_command(connection->rc_handle);
    }
    else
    {
	  printf("Connection is NULL when get element attr\n");
	  return -1;
    }

    while((get_element_flag == 0) && (times < 300)){
        usleep(100*1000);
        times++;
    }

    if(get_element_flag == 0){
		printf("Warning: get element attr timeout\n");
        return -1;
    }

	*p_s_avk_element_attr = s_avk_element_attr;
	return 0;
}

void s_hs_pick_up()
{
    app_hs_answer_call();
}

void s_hs_hung_up()
{
    app_hs_hangup();
}

void stop_app_avk()
{
    tBSA_AVK_DISABLE disable_param;

    app_avk_deregister();
    /* Terminate the avk profile */
    BSA_AvkDisableInit(&disable_param);
    BSA_AvkDisable(&disable_param);

    avk_start_status = 0;
}

void stop_app_hs()
{
    /* Stop Headset service*/
    app_hs_stop();
}

void bluetooth_stop()
{
    app_mgt_close();
    bluetooth_on = 0;
}
