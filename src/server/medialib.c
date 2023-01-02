#include <fcntl.h>
#include <sys/types.h>
#include <proto.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "medialib.h"
#include "mytbf.h"
#include "server_conf.h"

#define PATHSIZE     1024
#define LINEBUFFSIZE 1024

struct channel_context_st
{
    chnid_t chnid;
    char *desc;
    glob_t mp3glob;
    int pos;
    int fd;
    off_t offset;
    mytbf_t *tbf;
};

static struct channel_context_st channel[MAXCHNID + 1];

static struct channel_context_st *path2entry(const char *path)
{
    char pathstr[PATHSIZE];
    char linebuf[LINEBUFFSIZE];
    FILE *fp;
    struct channel_context_st *me;
    static chnid_t curr_id = MINCHNID;

    strncpy(pathstr,path,PATHSIZE);
    strncat(pathstr,"/desc.text",PATHSIZE);
    fp = fopen(pathstr,"r");
    if(fp ==NULL)
    {
        syslog(LOG_INFO, "%s is not a channel dir(Can't find desc.text)",path);
        return NULL;
    }
    if(fgets(linebuf,LINEBUFFSIZE,fp) ==NULL)
    {
        syslog(LOG_INFO,"%s is not a channel dir(Cant't get the desc.text)",path);
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    me = malloc(sizeof(struct channel_context_st));
    if(me ==NULL)
    {
        syslog(LOG_ERR,"malloc():%s\n",strerror(errno));
        return NULL;
    }

    me->tbf = mytbf_init(MP3_BITRATE/9*6,MP3_BITRATE/8*13);
    
    if(me->tbf ==NULL)
    {
        syslog(LOG_ERR,"mytbf_init():%s\n",strerror(errno));
        free(me);
        return NULL;
    }
    
    me->desc = strdup(linebuf);

    strncpy(pathstr,path,PATHSIZE);
    strncat(pathstr,"/*.mp3",PATHSIZE);
    if(glob(pathstr,0,NULL,&me->mp3glob) != 0)
    {
        curr_id ++;
        syslog(LOG_ERR,"%s is not a channel dir(Can't find mp3 files)",path);
        free(me);
        return NULL;
    }
    
    me->pos = 0;
    me->offset = 0;
    me->fd = open(me->mp3glob.gl_pathv[me->pos],O_RDONLY);
    if(me->fd < 0)
    {
        syslog(LOG_WARNING,"%s open failed.",me->mp3glob.gl_pathv[me->pos]);
        free(me);
        return NULL;
    }

    me->chnid = curr_id;
    curr_id++;
    return me;
}

int mlib_getchnlist(struct mlib_listentry_st **result,int *resnum)
{
    int i;
    char path[PATHSIZE];
    glob_t globres;
    int num =0;
    struct mlib_listentry_st *ptr;
    struct channel_context_st *res;

    for(i = 0;i < MAXCHNID+1 ;i++)
    {
        channel[i].chnid = -1; 

    }
    snprintf(path,PATHSIZE,"%s/*",server_conf.media_dir);

    if(glob(path,0,NULL,&globres) < 0)
    {
        return -1;
    }

    ptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
    if(ptr ==NULL)
    {
        syslog(LOG_ERR,"malloc() error.");
        exit(1);
    }

    for(i = 0;i < globres.gl_pathc;i++)
    {
        res = path2entry(globres.gl_pathv[i]);
        if(res != NULL)
        {
            syslog(LOG_DEBUG,"path2entry() returned :%d %s.",res->chnid,res->desc);
            memcpy(channel + res->chnid,res,sizeof(*res));
            ptr[num].chnid = res->chnid;
            ptr[num].desc = strdup(res->desc);
            num++;
        }
        
    }
    *result = realloc(ptr,sizeof(struct mlib_listentry_st) * num);
    if(*result ==NULL)
    { 
        syslog(LOG_ERR,"realloc() failed.");
    }
    *resnum = num;
    return 0;
}

int mlib_freechnlist(struct mlib_listentry_st *ptr)
{
    free(ptr);
}

static int open_next(chnid_t chnid)
{
    int i;
    for(i = 0; i < channel[chnid].mp3glob.gl_pathc; i++)
    {    channel[chnid].pos++;

        if(channel[chnid].pos == channel[chnid].mp3glob.gl_pathc)
        {
            channel[chnid].pos = 0;
            break;
        }

        close(channel[chnid].fd);
        channel[chnid].fd = open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],O_RDONLY);
        if(channel[chnid].fd < 0)
        {
            syslog(LOG_WARNING,"open(%s):%s",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
        }
        else
        {
            channel[chnid].offset = 0;
            return 0;
        }
    }
    syslog(LOG_ERR,"None of mp3s in channel %d is available.",chnid);
}

ssize_t mlib_readchn(chnid_t chnid,void *buf,size_t size)
{
    int tbfsize;
    int len;
    tbfsize = mytbf_fetchtocken(channel[chnid].tbf,size);
    while(1)
    {
        len = pread(channel[chnid].fd,buf, tbfsize, channel[chnid].offset);
        if(len < 0)
        {
            syslog(LOG_WARNING,"media file %s pread():%s",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
            open_next(chnid);
        }
        else if(len == 0)
        {
            syslog(LOG_DEBUG,"media file %s is over.",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
            open_next(chnid);
        }
        else{
            channel[chnid].offset += len;
            break;
        }
    }
    if(tbfsize - len > 0)
        mytbf_returntocken(channel[chnid].tbf,tbfsize - len);
    
    return len;
}