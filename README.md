# music-broadcast
音乐广播
centos7虚拟机环境运行
![img]https://github.com/ymn456/music-broadcast/blob/main/demo.gif
使用UDP广播套接字通信，服务端使用多线程并发同时往外发送多个频道信息。令牌桶和线程锁控制接收信息速率，使得能够正常播放。
