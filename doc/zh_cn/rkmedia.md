***
rkmedia 封装库
=============

简介
----

rkmedia为了使多媒体相关开发更简单而做，将比较偏底层一些的代码或功能抽象简化为更简易的接口。

目前包含了视频硬件编解码接口，媒体格式封装解封装接口，音频软件编解码接口，音频采集输出播放接口等。

编译基于[cmake](https://cmake.org/documentation)构建工具。

视频硬件编码
----------

- 编译

    确保对应CMakeLists.txt设置-DRKMPP=ON

- 范例：[mpp_enc_test.cc](../../frameworks/media/rkmpp/test/mpp_enc_test.cc)

    使用命令查看使用方法：./rkmpp_test -? （可能默认生成的固件里没有此可执行bin，需要到pc上生成的路径手动push到板端）。

- 接口及范例流程说明

    * (可不调用)rkmedia::REFLECTOR(Encoder)::DumpFactories() ：列出当前编入的编码模块
    * rkmedia::REFLECTOR(Encoder)::Create\<rkmedia::VideoEncoder\> ：创建视频编码器实例，参数为上述的DumpFactories中列出的一个模块对应的字符串
    * InitConfig：初始化编码器，参数为所需编码算法对应的设置系数
    * GetExtraData：获取参数信息数据，h264的pps和sps数据在此返回的buffer里
    * Process：执行编码，参数为原始未压缩图像数据buffer、压缩图像输出的buffer以及额外的输出buffer（如需要h264中的mv数据，在此buffer输出）  
    注意的是，调用此函数前，需要给所有buffer SetValidSize表明buffer可访问的长度空间。最后输出buffer的数据长度以GetValidSize体现。

媒体格式解封装
------------

- 编译

    确保对应CMakeLists.txt设置-D\<FORMAT\>=ON -D\<FORMAT\>_DEMUXER=ON  
    比如ogg音频解封装解码oggvorbis，就是-DOGGVORBIS=ON -DOGGVORBIS_DEMUXER=ON

- 范例：[ogg_decode_test.cc](../../frameworks/media/ogg/test/ogg_decode_test.cc)

    使用命令查看使用方法：./rkogg_decode_test -? （可能默认生成的固件里没有此可执行bin，需要到pc上生成的路径手动push到板端）。

- 接口及范例流程说明

    * (可不调用)rkmedia::REFLECTOR(Demuxer)::DumpFactories()：列出当前编入的格式解封装模块
    * rkmedia::REFLECTOR(Demuxer)::Create\<rkmedia::Demuxer\>：创建格式解封装实例，参数为上述的DumpFactories中列出的一个模块对应的字符串和其他一些设置项
    * IncludeDecoder()：判断是否此解封装模块已包含了解码功能。范例中oggvorbis比较特殊，包含了解码功能，否则需要按章节[音频解码](#音频解码)所述创建解码器实例为后续解封装出来的对应数据做解码
    * Init(Stream *input, MediaConfig *out_cfg)：设置输入流和获取音频参数
    * Read：读取一次数据

音频播放输出
----------

> RK目前仅支持alsa，后续可能支持pulseaudio

- 编译

    确保对应CMakeLists.txt设置-DALSA_PLAYBACK=ON

- 范例：[ogg_decode_test.cc](../../frameworks/media/ogg/test/ogg_decode_test.cc)，复用[媒体格式解封装范例](#媒体格式解封装)一样的范例

    使用命令：./rkogg_decode_test -i test.ogg -o alsa:default

- 接口及范例流程说明

    * (可不调用)rkmedia::REFLECTOR(Stream)::DumpFactories()：列出当前编入输入输出模块，stream的操作方法参数与c的FILE类似
    * rkmedia::REFLECTOR(Stream)::Create\<rkmedia::Stream\>：创建音频播放输出流实例，参数为字符串"alsa_playback_stream"和设置打开设备的参数。参数必须如范例中所示，包含device=\*\\nformat=\*\\nchannels=\*\\nsample_rate=\*\\n
    * Write：写入一次数据，参数为buffer对应的frame size和frame numbers
    * Close：关闭输出流

音频输入采集
----------

> RK目前仅支持alsa，后续可能支持pulseaudio

- 编译

    确保对应CMakeLists.txt设置-DALSA_CAPTURE=ON

- 范例：[ogg_encode_test.cc](../../frameworks/media/ogg/test/ogg_encode_test.cc)， 复用[媒体格式封装范例](#媒体格式封装)一样的范例

    使用命令：./rkogg_encode_test -f s16le -c 2 -r 48000 -i alsa:default -o output_s16le_c2_r48k.pcm

- 接口及范例流程说明

    * (可不调用)rkmedia::REFLECTOR(Stream)::DumpFactories()：列出当前编入输入输出模块，stream的操作方法参数与c的FILE类似
    * rkmedia::REFLECTOR(Stream)::Create\<rkmedia::Stream\>：创建音频播放采集流实例，参数为字符串"alsa_capture_stream"和设置打开设备的参数。参数必须如范例中所示，包含device=\*\\nformat=\*\\nchannels=\*\\nsample_rate=\*\\n
    * Read：读入一次数据，参数为buffer以及其对应的frame size和frame numbers
    * Close：关闭采集流


音频编码
-------

- 编译

    确保对应CMakeLists.txt设置-D\<FORMAT\>_ENCODER=ON  
    比如VORBIS音频编码vorbis，就是-DOGGVORBIS=ON -DVORBIS_ENCODER=ON

- 范例：[ogg_encode_test.cc](../../frameworks/media/ogg/test/ogg_encode_test.cc)

    使用命令查看使用方法：./ogg_encode_test -? （可能默认生成的固件里没有此可执行bin，需要到pc上生成的路径手动push到板端）。

- 接口及范例流程说明

    * (可不调用)rkmedia::REFLECTOR(Encoder)::DumpFactories()：列出当前编入的编码器模块
    * rkmedia::REFLECTOR(Encoder)::Create\<rkmedia::AudioEncoder\>：创建音频编码器实例，参数为上述的DumpFactories中列出的一个模块对应的字符串，比如范例中的libvorbisenc
    * InitConfig：初始化编码器，参数为所需编码算法对应的设置系数
    * Process：如果该函数返回负值且errno==ENOSYS，则表示此编码器不支持该接口，需要调用下面的SendInput和FetchOutput接口
    * SendInput：传入裸数据SampleBuffer，如果frames为0，则表示告知编码器传入数据结束
    * FetchOutput：获取编码后的数据，由于有些编码器，比如libvorbisenc，输入一次，会输出多帧，所以这里需要while循环获取直到无数据

媒体格式封装
------------

- 编译

    确保对应CMakeLists.txt设置-D\<FORMAT\>_MUXER=ON  
    比如OGG音频封装ogg，就是-DOGGVORBIS=ON -DOGG_MUXER=ON

- 范例：[ogg_encode_test.cc](../../frameworks/media/ogg/test/ogg_encode_test.cc)

    使用命令查看使用方法：./rkogg_encode_test -? （可能默认生成的固件里没有此可执行bin，需要到pc上生成的路径手动push到板端）。

- 接口及范例流程说明

    * (可不调用)rkmedia::REFLECTOR(Muxer)::DumpFactories()：列出当前编入的格式封装模块
    * rkmedia::REFLECTOR(Muxer)::Create\<rkmedia::Muxer\>：创建格式封装实例，参数为上述的DumpFactories中列出的一个模块对应的字符串，比如范例中的liboggmuxer
    * IncludeEncoder()：判断是否此解封装模块已包含了编码功能。如果没有，需要按章节[音频编码](#音频编码)所述创建编码器实例先做编码再传入做封装
    * NewMuxerStream：创建一路数据流，返回数据流对应序号于参数stream_no
    * SetIoStream：托管封装后数据写入的io stream。如果调用了该函数，则封装后立刻调用此io stream的Write方法写入数据；否则需要外部程序自行处理输出的封装数据
    * WriteHeader：获取封装格式头信息数据
    * Write：传入编码后的数据和对应数据流的序号，输出封装数据

rtsp服务端（基于live555）
----------------------

- 编译

    确保对应CMakeLists.txt设置-DLIVE555=ON -DLIVE555_SERVER=ON -DLIVE555_SERVER_H264=ON

- 范例：[rtsp_server_test.cc](../../frameworks/media/live555/server/test/rtsp_server_test.cc)

    拷贝对应的h264单帧数据[h264_frames](../../frameworks/media/live555/server/test/h264_frames)到板端文件夹备用。  
    使用命令查看使用方法：./rtsp_server_test -? （可能默认生成的固件里没有此可执行bin，需要到pc上生成的路径手动push到板端）。  
    在pc端播放验证时，注意最好板端是用有线网络，在很多EVB板子上无线wifi经常导致丢包问题。

- 范例流程说明

    以开启SIMPLE宏为例说明，此为纯粹的RTSP服务端功能范例。

    * split_h264_separate：分割多个slice为单独的slice，因为live555一次只接受一个slice。如范例中，sps和pps在一起，需要调用此函数进行分割
    * SetUserFlag/SetValidSize/SetTimeStamp：此三项必须设置，是后续必须的参数
    * rkmedia::REFLECTOR(Flow)::Create\<rkmedia::Flow\>(
      \"live555_rtsp_server\", param.c_str())：创建rtsp server，参数必须包括KEY_INPUTDATATYPE/KEY_CHANNEL_NAME，
      KEY_PORT_NUM（端口号）还有KEY_USERNAME/KEY_USERPASSWORD（用户名和密码）非必须。
    * rtsp_flow->SendInput(buf, 0)：送数据给rtsp服务端，第二个参数一般为0，表示送入rtsp服务端数据链0槽位，之后rtsp服务端内部会从0槽位获取数据。