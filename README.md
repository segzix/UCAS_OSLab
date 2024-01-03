# Project5 - DeviceDriver

## Test Sets

`send`：发消息测试，在命令行输入 `exec send `，测试集中设置的是发送4个包，可以用wireshark查看

`recv`：接收消息测试，在命令行输入 `exec recv`，接收时启动pktRxTx程序，可以选择接收60个包或接收60秒

`recv_stream`: C_core测试，启动ack和rsd机制

## Debug

1. 注意对于IO外设一定是实地址，用虚地址会报错
2. 注意main函数中的框架，e1000和plic的init
3. 双核上板中断会路由给两个核，第二个核进入内核时注意直接退出，此时对应中断位已经被清零
4. pktRxTx小程序有bug，注意进行重传时一是不要太过频繁，8秒即可；2是这时候不要采用中断唤醒机制，采用定时唤醒机制
