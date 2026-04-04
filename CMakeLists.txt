# CMAKE版本
cmake_minimum_required(VERSION 3.20)
# 项目名称
project(sylar)

# C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 配置编译选项
set(CMAKE_CXX_FLAGS_DEBUG "-g -rdynamic -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -fPIC")
# 启用位置无关代码（解决 relocation 警告）
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 配置最终的可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/exe)
# 设置库文件输出路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 查找 yaml-cpp 库
find_package(yaml-cpp REQUIRED CONFIG)

# 加载子目录
add_subdirectory(src)

option(ENABLE_TEST "Build test ON" ON)

if (ENABLE_TEST) 
    # 获得测试目录文件
    file(GLOB test ${CMAKE_SOURCE_DIR}/test/*.cpp)
    # 遍历所有测试文件，为每个生成一个可执行文件
    foreach(test_src ${test})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE lsylar) # 自动继承依赖
    endforeach()
endif()

option(BUILD_Gomoku_SERVER "Build gomoku server" OFF)
if (BUILD_Gomoku_SERVER)
    # 执行webapps
    # gomoku
    file(GLOB_RECURSE gomoku_src ${CMAKE_SOURCE_DIR}/webapps/gomoku/src/*.cpp)
    # 添加可执行文件（可执行名叫 gomoku）
    add_executable(gomoku ${gomoku_src})
    # 包含 gomoku 的头文件目录
    target_include_directories(gomoku PRIVATE ${CMAKE_SOURCE_DIR}/webapps/gomoku/include/base
                                        ${CMAKE_SOURCE_DIR}/webapps/gomoku/include/servlet)
    # 链接 sylar 主库或其他依赖库
    target_link_libraries(gomoku PRIVATE lsylar)
endif()


# ------------------------------------------------------------------
#                               chat_server                        |
# ------------------------------------------------------------------
option(BUILD_CHAT_SERVER "Build chat server" OFF)
option(ENABLE_AI_CURL "Enable curl support" OFF)
option(ENABLE_AI_OPENCV "Enable OpenCV support" OFF)
option(ENABLE_AI_ONNX "Enable ONNX Runtime support" OFF)
option(ENABLE_AI_MQ "Enable RabbitMQ support" OFF)

if(BUILD_CHAT_SERVER)
    file(GLOB_RECURSE chat_server_src
        ${CMAKE_SOURCE_DIR}/webapps/chatServer/src/*.cpp
    )

    add_executable(chat_server ${chat_server_src})

    target_include_directories(chat_server PRIVATE
        ${CMAKE_SOURCE_DIR}/webapps/chatServer/include/base
        ${CMAKE_SOURCE_DIR}/webapps/chatServer/include/servlet
        ${CMAKE_SOURCE_DIR}/webapps/chatServer/include/service
    )

    target_compile_definitions(chat_server PRIVATE
        SYLAR_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
        SYLAR_CHATSERVER_ROOT="${CMAKE_SOURCE_DIR}/webapps/chatServer"
        SYLAR_CHATSERVER_RESOURCE_DIR="${CMAKE_SOURCE_DIR}/webapps/chatServer/resource"
        SYLAR_CHATSERVER_SSL_CERT="${CMAKE_SOURCE_DIR}/bin/conf/server.crt"
        SYLAR_CHATSERVER_SSL_KEY="${CMAKE_SOURCE_DIR}/bin/conf/server.key"
    )

    target_link_libraries(chat_server PRIVATE lsylar)

    if(ENABLE_AI_CURL)
        find_package(CURL REQUIRED)
        target_link_libraries(chat_server PRIVATE CURL::libcurl)
        target_compile_definitions(chat_server PRIVATE SYLAR_ENABLE_AI_CURL=1)
    endif()

    if(ENABLE_AI_OPENCV)
        find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui imgcodecs videoio dnn)
        target_include_directories(chat_server PRIVATE ${OpenCV_INCLUDE_DIRS})
        target_link_libraries(chat_server PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(chat_server PRIVATE SYLAR_ENABLE_AI_OPENCV=1)
    endif()

    if(ENABLE_AI_ONNX)
        find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
            PATHS /usr/include /usr/local/include /usr/local/include/onnxruntime /usr/local/include/onnxruntime/core/session
        )
        find_library(ONNXRUNTIME_LIBRARY onnxruntime
            PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
        )

        if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
            message(FATAL_ERROR "ONNX Runtime not found")
        endif()

        target_include_directories(chat_server PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(chat_server PRIVATE ${ONNXRUNTIME_LIBRARY})
        target_compile_definitions(chat_server PRIVATE SYLAR_ENABLE_AI_ONNX=1)
    endif()

    if(ENABLE_AI_MQ)
        find_path(SimpleAmqpClient_INCLUDE_DIR SimpleAmqpClient/SimpleAmqpClient.h
            PATHS /usr/include /usr/local/include
        )
        find_library(SimpleAmqpClient_LIBRARY SimpleAmqpClient
            PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
        )
        find_library(RABBITMQ_LIBRARY rabbitmq
            PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
        )

        if(NOT SimpleAmqpClient_INCLUDE_DIR OR NOT SimpleAmqpClient_LIBRARY OR NOT RABBITMQ_LIBRARY)
            message(FATAL_ERROR "RabbitMQ / SimpleAmqpClient not found")
        endif()

        target_include_directories(chat_server PRIVATE ${SimpleAmqpClient_INCLUDE_DIR})
        target_link_libraries(chat_server PRIVATE
            ${SimpleAmqpClient_LIBRARY}
            ${RABBITMQ_LIBRARY}
        )
        target_compile_definitions(chat_server PRIVATE SYLAR_ENABLE_AI_MQ=1)
    endif()
endif()
