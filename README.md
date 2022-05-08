# kanon_httpd
## Introduction
`kanon_httpd` is a simple http server. The server can only run on linux/unix-like platform only since does so `kanon`.
It can accept and process http request including `GET`, `POST`. 
Also, it support long connection and short connection(i.e. `keep-alive` header).
Compared to the old approach `CGI` to back dynamic pages, I apply the modern approach `Shared object` as `plugin` to provide them. The content provider need implemetes the [interface class](https://github.com/Conzxy/kanon_httpd/blob/master/src/plugin/http_dynamic_response_interface.h) to the `*/kanon_httpd/resource/contents` only, then build tool will generate `plugin` automatically.

## Options
The server has some options, you can type `-h` to look it:
```shell
$ httpd_kanon -h/--help
Allowed options:
  -h [ --help ]         help about kanon httpd
  -p [ --port ] arg     Tcp port number of httpd(default: 80)
  -t [ --threads ] arg  Threads number of IO thread instead of main
                        thread(default: 0)
  -c [ --config ] arg   Configuration file path(default: ./kanon_httpd.conf)
  -l [ --log ] arg      How to log: terminal, file(default: terminal)
  -d [ --log_dir ] arg  The directory where log file store(default:
                        ${HOME}/.log)
```
* The default port is `80`, but you can modify it in the range of unsigned 16 bit integer.
* The number of other `IO threads` is `0` default, you can set to non-zero, then main thread just accept new connection and other threads handle IO events, this is supported by `kanon`.
* I provide a configuration file, but you should change its contents, and the default search location is `.kanon_httpd.conf` in current working directory(i.e. `./`), so you should put executable file with it to same directory.
* The output of log information can be terminal or file, and you can privide the log location that is `${HOME}/.log` default, this is also support by `kanon`.

## Build
I wrote two version of it, but the version 1 has not been maintained. Therefore the version2 is the default build target.

This project depends on [`kanon`](https://github.com/Conzxy/kanon) and [`boost`](https://www.boost.org/)(Only the `program_options` module of it). You must install them first. Besides, The project is built by `cmake`，so you also need to install `cmake`.

I prepare a shell script in the project directory, it should meet the basic requirements of build, also you can modify it.
```shell
mkdir build && cd build && \
# Options:
# -DBUILD_ALL_TESTS=ON(default: OFF)
# -DBUILD_STATIC_LIBS=ON(default: OFF)
# -G Ninja(default: Unix Makefiles), ninja need install
# -DCMAKE_BUILD_TYPE=Debug/...(default: Release)
cmake .. && \
cmake --build . -j 2 --target http_common && \
cmake --build . -j 2 --target httpd_kanon && \
cmake -DGEN_PLUGIN=ON ..
# Then, you can check ./lib/ if has three *.so files
# , ../resources/contents/adder and ../bin/httpd_kanon
# if does exists, then run the ../bin/httpd_kanon
# (You should can the working directory to ../bin and 
#  modify the ../bin/kanon_httpd.conf)
```

## Run Test
Open a web browser, and input the URL: `http://Hostname/` will access the home page.
I prepare one dynamic page: `adder` that is very simple page.
You can access this page by following approach:
* `GET` method: `http://Hostname/contents/adder?a=*&b=*`
* `POST` method: `http://Hostname/html/post_test.html/` and fill the form and submit, then httpd will back dynamic page like `GET` approach.

## TODO
