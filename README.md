# kanon_httpd
## Introduction
`kanon_httpd`是一个简易http服务器。其网络模块依赖于个人编写的[kanon](https://github.com/Conzxy/kanon)网络库，因此只能在**linux/unix-like**平台上运行（暂不考虑跨平台）。<br>
该服务器可以接受和处理http请求并返回静态或动态页面。同时，也支持长连接和短连接(i.e. `keep-alive`)。

相较于传统的`CGI + fork-and-execute`模型返回动态页面，我使用较为现代的方式：将**共享目标文件(shared object file, e.g. \*.so)**作为`插件(plugin)`来提供这类服务。

动态内容的提供者需要实现[准备好的的接口类](https://github.com/Conzxy/kanon_httpd/blob/master/src/plugin/http_dynamic_response_interface.h)，至于生成的共享文件放哪取决于你自己，包括生成方法。

| Approach | Advantage | Disadvantage |
| -- | --- | -- |
| CGI + fork-and-exec| 实现简单；语言无关 | 性能低下（每个请求都得调用fork()和execve()） |
| Shared object | 性能最好（本质是一次函数调用） | 语言相关，若其他语言要支持受其标准约束 |
| Fast CGI | 性能相比`fork-and-exec`有所提升（一定程度缓解了传统CGI的问题）；允许独立于本机，即可以支持分布式部署；语言无关 | 实现较为复杂 |

## Server Options
该服务器支持一些命令行选项，你可以通过键入`--help`查看详细的描述。<br>
(命令行选项由个人编写的[takina](https://github.com/Conzxy/takina)支持)
```shell
$ ./httpd --help
```

## Build
该项目依赖于[kanon](https://github.com/Conzxy/kanon)。根据它的github页面进行安装，同时，`kanon`和该项目都是通过`cmake`构筑的，因此你还得安装`cmake`。

在项目根目录有一个shell脚本来构筑整个项目，你也根据自己的需要取修改它。

## Run & Test
打开Web浏览器，并输入URL: `http://Hostname/`将会访问其主页。<br>
我准备了一个简单的动态页面: `adder`。<br>
你可以通过以下方式访问它：
* `GET`: `http://Hostname/contents/adder?a=*&b=*`
* `POST`: `http://Hostname/html/post_test.html/`

## TODO
* Fast CGI