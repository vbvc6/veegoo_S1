#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <sys/select.h>

#include <allwinner/tinaplayer.h>
#include <power_manager_client.h>

using namespace aw;
#define SAVE_YUV_DATA 1

typedef unsigned long uintptr_t ;

typedef struct DemoPlayerContext
{
    TinaPlayer*       mTinaPlayer;
    int               mSeekable;
	int               mError;
    int               mVideoFrameNum;
}DemoPlayerContext;


//* define commands for user control.
typedef struct Command
{
    const char* strCommand;
    int         nCommandId;
    const char* strHelpMsg;
}Command;

#define COMMAND_HELP            0x1     //* show help message.
#define COMMAND_QUIT            0x2     //* quit this program.

#define COMMAND_SET_SOURCE      0x101   //* set url of media file.
#define COMMAND_PREPARE		0x102   //* prepare the media file.
#define COMMAND_PLAY            0x103   //* start playback.
#define COMMAND_PAUSE           0x104   //* pause the playback.
#define COMMAND_STOP            0x105   //* stop the playback.
#define COMMAND_SEEKTO          0x106   //* seek to posion, in unit of second.
#define COMMAND_RESET           0x107   //* reset the player
#define COMMAND_SHOW_MEDIAINFO  0x108   //* show media information.
#define COMMAND_SHOW_DURATION   0x109   //* show media duration, in unit of second.
#define COMMAND_SHOW_POSITION   0x110   //* show current play position, in unit of second.
#define COMMAND_SWITCH_AUDIO    0x111   //* switch autio track.

#define CEDARX_UNUSE(param) (void)param

static const Command commands[] =
{
    {"help",            COMMAND_HELP,               "show this help message."},
    {"quit",            COMMAND_QUIT,               "quit this program."},
    {"set url",         COMMAND_SET_SOURCE,         "set url of the media, for example, set url: ~/testfile.mkv."},
    {"prepare",         COMMAND_PREPARE,		"prepare the media."},
    {"play",            COMMAND_PLAY,               "start playback."},
    {"pause",           COMMAND_PAUSE,              "pause the playback."},
    {"stop",            COMMAND_STOP,               "stop the playback."},
    {"seek to",         COMMAND_SEEKTO,
            "seek to specific position to play, position is in unit of second, for example, seek to: 100."},
    {"reset",		COMMAND_RESET,               "reset the player."},
    {"show media info", COMMAND_SHOW_MEDIAINFO,     "show media information of the media file."},
    {"show duration",   COMMAND_SHOW_DURATION,      "show duration of the media file."},
    {"show position",   COMMAND_SHOW_POSITION,      "show current play position, position is in unit of second."},
    {"switch audio",    COMMAND_SWITCH_AUDIO,
        "switch audio to a specific track, for example, switch audio: 2, track is start counting from 0."},
    {NULL, 0, NULL}
};

static void showHelp(void)
{
    int     i;

    printf("\n");
    printf("******************************************************************************************\n");
    printf("* This is a simple media player, when it is started, you can input commands to tell\n");
    printf("* what you want it to do.\n");
    printf("* Usage: \n");
    printf("*   # ./tinaplayerdemo\n");
    printf("*   # set url: http://www.allwinner.com/ald/al3/testvideo1.mp4\n");
	printf("*   # prepare\n");
    printf("*   # show media info\n");
    printf("*   # play\n");
    printf("*   # pause\n");
    printf("*   # stop\n");
	printf("*   # reset\n");
	printf("*   # seek to:100\n");
    printf("*\n");
    printf("* Command and it param is seperated by a colon, param is optional, as below:\n");
    printf("*     Command[: Param]\n");
    printf("*\n");
    printf("* here are the commands supported:\n");

    for(i=0; ; i++)
    {
        if(commands[i].strCommand == NULL)
            break;
        printf("*    %s:\n", commands[i].strCommand);
        printf("*\t\t%s\n",  commands[i].strHelpMsg);
    }
    printf("*\n");
    printf("******************************************************************************************\n");
}

static int readCommand(char* strCommandLine, int nMaxLineSize)
{
    int            nMaxFds;
    fd_set         readFdSet;
    int            result;
    char*          p;
    unsigned int   nReadBytes;

    printf("\ntinademoPlayer:readCommand() \n");
    fflush(stdout);

    nMaxFds    = 0;
    FD_ZERO(&readFdSet);
    FD_SET(STDIN_FILENO, &readFdSet);

    result = select(nMaxFds+1, &readFdSet, NULL, NULL, NULL);
    if(result > 0)
    {
        if(FD_ISSET(STDIN_FILENO, &readFdSet))
        {
		nReadBytes = read(STDIN_FILENO, &strCommandLine[0], nMaxLineSize);
		if(nReadBytes > 0)
		{
		    p = strCommandLine;
		    while(*p != 0)
		    {
		        if(*p == 0xa)
		        {
		            *p = 0;
		            break;
		        }
		        p++;
		    }
		}

		return 0;
        }
	}

	return -1;
}

static void formatString(char* strIn)
{
    char* ptrIn;
    char* ptrOut;
    int   len;
    int   i;

    if(strIn == NULL || (len=strlen(strIn)) == 0)
        return;

    ptrIn  = strIn;
    ptrOut = strIn;
    i      = 0;
    while(*ptrIn != '\0')
    {
        //* skip the beginning space or multiple space between words.
        if(*ptrIn != ' ' || (i!=0 && *(ptrOut-1)!=' '))
        {
            *ptrOut++ = *ptrIn++;
            i++;
        }
        else
            ptrIn++;
    }

    //* skip the space at the tail.
    if(i==0 || *(ptrOut-1) != ' ')
        *ptrOut = '\0';
    else
        *(ptrOut-1) = '\0';

    return;
}


//* return command id,
static int parseCommandLine(char* strCommandLine, int* pParam)
{
    char* strCommand;
    char* strParam;
    int   i;
    int   nCommandId;
    char  colon = ':';

    if(strCommandLine == NULL || strlen(strCommandLine) == 0)
    {
        return -1;
    }

    strCommand = strCommandLine;
    strParam   = strchr(strCommandLine, colon);
    if(strParam != NULL)
    {
        *strParam = '\0';
        strParam++;
    }

    formatString(strCommand);
    formatString(strParam);

    nCommandId = -1;
    for(i=0; commands[i].strCommand != NULL; i++)
    {
        if(strcmp(commands[i].strCommand, strCommand) == 0)
        {
            nCommandId = commands[i].nCommandId;
            break;
        }
    }

    if(commands[i].strCommand == NULL)
        return -1;

    switch(nCommandId)
    {
        case COMMAND_SET_SOURCE:
            if(strParam != NULL && strlen(strParam) > 0)
                *pParam = (uintptr_t)strParam;        //* pointer to the url.
            else
            {
                printf("no url specified.\n");
                nCommandId = -1;
            }
            break;

        case COMMAND_SEEKTO:
            if(strParam != NULL)
            {
                *pParam = (int)strtol(strParam, (char**)NULL, 10);  //* seek time in unit of second.
                if(errno == EINVAL || errno == ERANGE)
                {
                    printf("seek time is not valid.\n");
                    nCommandId = -1;
                }
            }
            else
            {
                printf("no seek time is specified.\n");
                nCommandId = -1;
            }
            break;

        case COMMAND_SWITCH_AUDIO:
            if(strParam != NULL)
            {
                *pParam = (int)strtol(strParam, (char**)NULL, 10);  //* audio stream index start counting from 0.
                if(errno == EINVAL || errno == ERANGE)
                {
                    printf("audio stream index is not valid.\n");
                    nCommandId = -1;
                }
            }
            else
            {
                printf("no audio stream index is specified.\n");
                nCommandId = -1;
            }
            break;

        default:
            break;
    }

    return nCommandId;
}


//* a callback for tinaplayer.
void CallbackForTinaPlayer(void* pUserData, int msg, int param0, void* param1)
{
    DemoPlayerContext* pDemoPlayer = (DemoPlayerContext*)pUserData;

	CEDARX_UNUSE(param1);

    switch(msg)
    {
        case TINA_NOTIFY_NOT_SEEKABLE:
        {
            pDemoPlayer->mSeekable = 0;
            printf("info: media source is unseekable.\n");
            break;
        }

        case TINA_NOTIFY_ERROR:
        {
            pDemoPlayer->mError = 1;
            printf("error: open media source fail.\n");
            break;
        }

        case TINA_NOTIFY_PREPARED:
        {
			printf("TINA_NOTIFY_PREPARED,has prepared.\n");
            break;
        }

        case TINA_NOTIFY_BUFFERRING_UPDATE:
        {
            int nBufferedFilePos;
            int nBufferFullness;

            nBufferedFilePos = param0 & 0x0000ffff;
            nBufferFullness  = (param0>>16) & 0x0000ffff;
            printf("info: buffer %d percent of the media file, buffer fullness = %d percent.\n",
                nBufferedFilePos, nBufferFullness);

            break;
        }

        case TINA_NOTIFY_PLAYBACK_COMPLETE:
        {
            printf("TINA_NOTIFY_PLAYBACK_COMPLETE\n");
            PowerManagerReleaseWakeLock("tinaplayerdemo");
            break;
        }

        case TINA_NOTIFY_SEEK_COMPLETE:
        {
            printf("TINA_NOTIFY_SEEK_COMPLETE>>>>info: seek ok.\n");
            break;
        }

		case TINA_NOTIFY_VIDEO_FRAME:
		{
			#if SAVE_YUV_DATA
				VideoPicData* videodata = (VideoPicData*)param1;
				if(videodata){
					//printf("*****TINA_NOTIFY_VIDEO_FRAME****,videodata->nPts = %lld ms",videodata->nPts/1000);
					if(pDemoPlayer->mVideoFrameNum == 200){
						printf(" *****TINA_NOTIFY_VIDEO_FRAME****,videodata->ePixelFormat = %d,videodata->nWidth = %d,videodata->nHeight=%d\n",videodata->ePixelFormat,videodata->nWidth,videodata->nHeight);
						char path[50];
						char width[10];
						char height[10];
						sprintf(width,"%d",videodata->nWidth);
						sprintf(height,"%d",videodata->nHeight);
						printf("width = %s,height = %s\n",width,height);
						strcpy(path,"/tmp/save_");
						strcat(path,width);
						strcat(path,"_");
						strcat(path,height);
						strcat(path,".yuv");
						FILE* savaYuvFd = fopen(path, "wb");
						if(savaYuvFd==NULL){
							printf("fopen save.yuv fail****\n");
							printf("err str: %s\n",strerror(errno));
						}else{
							fseek(savaYuvFd,0,SEEK_SET);
							int write_ret0 = fwrite(videodata->pData0, 1, videodata->nWidth*videodata->nHeight, savaYuvFd);
							if(write_ret0 <= 0){
								printf("yuv write0 error,err str: %s\n",strerror(errno));
							}
							int write_ret1 = fwrite(videodata->pData1, 1, videodata->nWidth*videodata->nHeight/2, savaYuvFd);
							if(write_ret1 <= 0){
								printf("yuv write1 error,err str: %s\n",strerror(errno));
							}
							printf("only save 1 video frame\n");
							fclose(savaYuvFd);
							savaYuvFd = NULL;
						}
					}
					pDemoPlayer->mVideoFrameNum++;
				}
			#endif
			break;
		}

		case TINA_NOTIFY_AUDIO_FRAME:
		{
			AudioPcmData* pcmData = (AudioPcmData*)param1;
			if(pcmData){
				//printf(" *****TINA_NOTIFY_AUDIO_FRAME#####,*pcmData->pData = %p,pcmData->nSize = %d\n",*(pcmData->pData),pcmData->nSize);
			}
			break;
		}

        default:
        {
            printf("warning: unknown callback from Tinaplayer.\n");
            break;
        }
    }

    return;
}


//* the main method.
int main(int argc, char** argv)
{
    DemoPlayerContext demoPlayer;
    int  nCommandId;
    int  nCommandParam;
    int  bQuit;
    char strCommandLine[1024];
	CEDARX_UNUSE(argc);
	CEDARX_UNUSE(argv);

    printf("\n");
    printf("******************************************************************************************\n");
    printf("* This program implements a simple player, you can type commands to control the player.\n");
    printf("* To show what commands supported, type 'help'.\n");
    printf("******************************************************************************************\n");

    //* create a player.
    memset(&demoPlayer, 0, sizeof(DemoPlayerContext));
    demoPlayer.mTinaPlayer= new TinaPlayer();
    if(demoPlayer.mTinaPlayer == NULL)
    {
        printf("can not create tinaplayer, quit.\n");
        exit(-1);
    }
    //* set callback to player.
    demoPlayer.mTinaPlayer->setNotifyCallback(CallbackForTinaPlayer, (void*)&demoPlayer);

    //* check if the player work.
    if(demoPlayer.mTinaPlayer->initCheck() != 0)
    {
        printf("initCheck of the player fail, quit.\n");
        delete demoPlayer.mTinaPlayer;
	    demoPlayer.mTinaPlayer = NULL;
        exit(-1);
    }
	demoPlayer.mError = 0;
	demoPlayer.mSeekable = 1;
    //* read, parse and process command from user.
    bQuit = 0;
    while(!bQuit)
    {
        //* read command from stdin.
        if(readCommand(strCommandLine, sizeof(strCommandLine)) == 0)
        {
            //* parse command.
            nCommandParam = 0;
            nCommandId = parseCommandLine(strCommandLine, &nCommandParam);
            //* process command.
            switch(nCommandId)
            {
                case COMMAND_HELP:
                {
                    showHelp();
                    break;
                }

                case COMMAND_QUIT:
                {
					printf("COMMAND_QUIT\n");
                    bQuit = 1;
                    break;
                }

                case COMMAND_SET_SOURCE :   //* set url of media file.
                {
                    char* pUrl;
                    pUrl = (char*)(uintptr_t)nCommandParam;

					if(demoPlayer.mError == 1) //pre status is error,reset the player first
			        {
						printf("pre status is error,reset the tina player first.\n");
						demoPlayer.mTinaPlayer->reset();
						demoPlayer.mError = 0;
			        }

                    demoPlayer.mSeekable = 1;   //* if the media source is not seekable, this flag will be
                                                //* clear at the TINA_NOTIFY_NOT_SEEKABLE callback.
                    //* set url to the tinaplayer.
                    if(demoPlayer.mTinaPlayer->setDataSource((const char*)pUrl, NULL) != 0)
                    {
                        printf("tinaplayer::setDataSource() return fail.\n");
                        break;
                    }else{
						printf("setDataSource end\n");
					}
					break;
                }

				case COMMAND_PREPARE:
				{
					/*
					if(demoPlayer.mTinaPlayer->prepare() != 0){
						printf(" tinaplayer::prepare() return fail.\n");
						break;
					}
					printf("has prepared...\n");
					*/
                    if(demoPlayer.mTinaPlayer->prepareAsync() != 0)
                    {
                        printf(" tinaplayer::prepareAsync() return fail.\n");
                        break;
                    }else{
						printf("preparing...\n");
					}
                    break;
				}

                case COMMAND_PLAY:   //* start playback.
                {
					demoPlayer.mTinaPlayer->setLooping(1);//let the player into looping mode
					//* start the playback
					if(demoPlayer.mTinaPlayer->start() != 0)
                    {
                        printf("tinaplayer::start() return fail.\n");
                        break;
                    }else{
						printf("started.\n");
						PowerManagerAcquireWakeLock("tinaplayerdemo");
					}

                    break;
                }

                case COMMAND_PAUSE:   //* pause the playback.
                {
					if(demoPlayer.mTinaPlayer->pause() != 0)
                    {
                        printf("tinaplayer::pause() return fail.\n");
                        break;
                    }else{
						printf("paused.\n");
						PowerManagerReleaseWakeLock("tinaplayerdemo");
					}
                    break;
                }

                case COMMAND_STOP:   //* stop the playback.
                {
                    if(demoPlayer.mTinaPlayer->stop() != 0)
                    {
                        printf("tinaplayer::stop() return fail.\n");
                        break;
                    }else{
						PowerManagerReleaseWakeLock("tinaplayerdemo");
					}
                    break;
                }

                case COMMAND_SEEKTO:   //* seek to posion, in unit of second.
                {
                    int nSeekTimeMs;
                    int nDuration;
                    nSeekTimeMs = nCommandParam*1000;
					int ret = demoPlayer.mTinaPlayer->getDuration(&nDuration);
					printf("nSeekTimeMs = %d , nDuration = %d\n",nSeekTimeMs,nDuration);
                    if(ret != 0)
                    {
						printf("getDuration fail, unable to seek!\n");
                        break;
                    }

					if(nSeekTimeMs > nDuration){
						printf("seek time out of range, media duration = %u seconds.\n", nDuration/1000);
						break;
					}
                    if(demoPlayer.mSeekable == 0)
                    {
                        printf("media source is unseekable.\n");
                        break;
                    }
                    if(demoPlayer.mTinaPlayer->seekTo(nSeekTimeMs) != 0){
						printf("tinaplayer::seekTo() return fail.\n");
                        break;
					}else{
						printf("is seeking.\n");
					}
                    break;
                }

				case COMMAND_RESET:   //* reset the player
                {
                    if(demoPlayer.mTinaPlayer->reset() != 0)
                    {
                        printf("tinaplayer::reset() return fail.\n");
                        break;
                    }else{
						printf("reset the player ok.\n");
						PowerManagerReleaseWakeLock("tinaplayerdemo");
					}
                    break;
                }

                case COMMAND_SHOW_MEDIAINFO:   //* show media information.
                {
                    printf("show media information.\n");
                    break;
                }

                case COMMAND_SHOW_DURATION:   //* show media duration, in unit of second.
                {
                    int nDuration = 0;
                    if(demoPlayer.mTinaPlayer->getDuration(&nDuration) == 0)
                        printf("media duration = %u seconds.\n", nDuration/1000);
                    else
                        printf("fail to get media duration.\n");
                    break;
                }

                case COMMAND_SHOW_POSITION:   //* show current play position, in unit of second.
                {
                    int nPosition = 0;
                    if(demoPlayer.mTinaPlayer->getCurrentPosition(&nPosition) == 0)
                        printf("current position = %u seconds.\n", nPosition/1000);
                    else
                        printf("fail to get pisition.\n");
                    break;
                }

                case COMMAND_SWITCH_AUDIO:   //* switch autio track.
                {
                    int nAudioStreamIndex;
                    nAudioStreamIndex = nCommandParam;
                    printf("switch audio to the %dth track.\n", nAudioStreamIndex);
                    //* TODO
                    break;
                }

                default:
                {
                    if(strlen(strCommandLine) > 0)
                        printf("invalid command.\n");
                    break;
                }
            }
        }
	}

	printf("destroy TinaPlayer.\n");

	if(demoPlayer.mTinaPlayer != NULL)
	{
	    delete demoPlayer.mTinaPlayer;
	    demoPlayer.mTinaPlayer = NULL;
	}

	printf("destroy TinaPlayer 1.\n");
	PowerManagerReleaseWakeLock("tinaplayerdemo");
    printf("\n");
    printf("******************************************************************************************\n");
    printf("* Quit the program, goodbye!\n");
    printf("******************************************************************************************\n");
    printf("\n");

	return 0;
}
