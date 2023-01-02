#ifndef CLIENT_H__
#define CLIENT_H__

#define DEFAULT_PLAYERCMD    "/usr/bin/mpg123 - > ../t.text "

struct client_conf_st
{
    char *rcvport;
    char *mgroup;
    char *player_cmd;
};

struct client_conf_st client_conf;

#endif