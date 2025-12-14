find_package(Boost REQUIRED COMPONENTS system thread)

file(GLOB SCHEME_SRC_FILES src/*.cpp)

file(GLOB COMMON_SRC
    ${CMAKE_SOURCE_DIR}/common/src/*.cpp
)

foreach(file ${SCHEME_SRC_FILES})
    get_filename_component(filename ${file} NAME_WE)

    add_executable(${filename}_netSim ${file} ${COMMON_SRC} scheme/RTS.cpp)

    target_include_directories(${filename}_netSim PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/common/include
    )

    target_link_libraries(${filename}_netSim
            PRIVATE
            miracl_core
            gmp::gmp
            gmp::gmpxx
            benchmark::benchmark
            websocketpp
            ${Boost_LIBRARIES} pthread
    )
endforeach()


# === 自动处理 scripts 文件夹下的所有 .sh 脚本 ===
# 1. 找到所有 .sh 文件
file(GLOB SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.sh")

# 2. 遍历并复制
foreach(script ${SCRIPTS})
    get_filename_component(name ${script} NAME)

    # 复制到 build 目录 (COPYONLY 保证了只有文件变动时才复制)
    configure_file(${script} ${CMAKE_CURRENT_BINARY_DIR}/scripts/${name} COPYONLY)

    # 简单粗暴地赋予执行权限 (比 file(CHMOD) 写法短得多)
    execute_process(COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/scripts/${name})
endforeach()

# 3. 处理配置文件 (只复制，不需要执行权限) <--- 新增这部分
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/config.env"
        "${CMAKE_CURRENT_BINARY_DIR}/scripts/config.env"
        COPYONLY
)