
g++ -o0 -g -Wl,-subsystem,windows -DGLEW_STATIC -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=1^
    main.cpp files_view.cpp^
    .\sftp\sshtransport.cpp^
    uploader.cpp^
    .\sftp\sshsftp.cpp^
    .\sftp\sshchannel.cpp^
    .\sftp\sshlog.cpp^
    .\sftp\sshpacket.cpp^
    .\sftp\sshtools.cpp^
    .\sftp\sshutils.cpp^
    .\sftp\tcpclient_bom.cpp^
    .\sftp\tcpclient.cpp^
    .\sftp\utils.cpp^
    .\sftp\bcrypt\bcrypt_pbkdf.cpp^
    .\sftp\bcrypt\blowfish.cpp^
    .\libs\glew\src\glew.c^
    .\libs\imgui\imgui.cpp^
    .\libs\imgui\imgui_draw.cpp^
    .\libs\imgui\imgui_demo.cpp^
    .\libs\imgui\imgui_tables.cpp^
    .\libs\imgui\imgui_widgets.cpp^
    .\libs\imgui\backend\imgui_impl_opengl3.cpp^
    .\libs\imgui\backend\imgui_impl_sdl.cpp^
    -I.\sftp^
    -I.\sftp\bcrypt^
    -I.\libs\glew\include^
    -I.\libs\SDL2\include^
    -I.\libs\imgui^
    -I.\libs^
    -L.\libs\SDL2\lib^
    -L.\libs\curl\lib^
    -lmingw32^
    -lcurl^
    -lssl^
    -lcrypto^
    -lssh2^
    -lbcrypt^
    -lws2_32^
    -lwsock32^
    -lSDL2main^
    -lSDL2^
    -lopengl32^
    -lshell32^
     2>build.log
