# MontComm
Secure communication and remote management

## 项目架构及功能说明

### 架构说明

Comm包含以下模块： CommMngr、Server、Client和一些可以单独部署的小组件（redis/mongo）

#### 1. CommMngr

CommMngr一般暴露在公网（或部署在局域网），提供以下服务：

1. 提供api接口，为Client提供当前负载最小的Server地址（可使用安全服务，为数据使用对应Client公钥加密传输）
2. 管理Server，包括：查看Server当前机器健康状态、查看当前Server当前注册在线的所有Client、使能/关闭Server对Client提供服务、为Server提供身份认证服务
3. 出于安全性考虑，Server/Client的密钥对生成由自身负责，不由CommMngr生成。CommMngr只能保存/验证parnter/user的身份信息，不能涉入Server/Client的安全性
4. 为Client、Server提供注册服务，并导入公钥，生成id，将id-公钥作为唯一身份信息（写入mongo）
5. 为Client提供互通域管理服务，只有在互通域之中的Client才能相互传递消息（写入mongo）
6. 前后端通过api的方式交互，以前后端分离的形式部署。可将后端部署在私网，可使用nginx反向代理部署后端集群

#### 2. Server

Server可选暴露在公网或部署在局域网，提供以下服务：

1. 在注册之前，自动生成公私钥
2. 可以创建集群，多个Server作为负载均衡；或创建私有Server只为指定Client提供连接
3. 从CommMngr自动同步互通域，明确哪些Client可以相互转发消息
4. 对外暴露接口，提供给Client提供连接，且只接受在CommMngr上注册过的Client的注册
5. 对初次链接的Client进行双向挑战应答，进行身份识别和会话密钥协商
6. 为互通域内的Client转发消息，拒绝非互通域内的消息转发，转发过程可选protobug明文/SM4加密传输
7. 向CommMngr提供所在机器的健康情况，以及Server的工作情况（写入redis）

#### 3. Client

Client出于安全考虑，一般部署在局域网，提供以下服务：

1. 在注册之前，自动生成公私钥
2. 通过数字信封的形式，对CommMngr请求Server地址进行连接
3. 与Server双向挑战应答，进行身份认证和会话密钥协商
4. 注册后，并且添加互通域后，可以向指定Client发送消息
5. 对Server集群可以自动切换，在当前的Server连接出问题时，自动切换集群中其他备机

#### 4. Client_Qt

基于Qt框架的Client图形界面客户端，提供：
1. 与C++ Client相同的核心通信功能（注册、消息收发、SM4加密）
2. 基于QMainWindow的图形界面，支持消息展示和交互操作
3. 通过Client_Qt.pro qmake工程管理构建

#### 5. 数据库服务

数据库出于安全考虑，部署在私网，提供服务：

1. mongo保存Client的公钥消息、互通域信息，在Server启动时，读取互通域消息
2. redis保存Server的相关健康信息

### 功能说明

#### 1. CommMngr权限控制

* 角色：manager

  仅manager可以注册互通域、获取Server管理信息

* 角色：partner

  partner（Server）可以在注册界面进行Server的注册

* 角色：user

  user（Client）可以访问注册界面进行Client的注册

#### 2. Server注册

partner在CommMngr上登录后，可以注册Server，在后台机器上生成密钥对后，导出公钥注册到CommMngr，作为身份证明

#### 3. Client注册

Client在CommMngr上注册，在后台机器上生成密钥对后，导出公钥注册到CommMngr，公钥将在CommMngr的数据库留存

![注册流程](./PrjArchSrc/Register.png)

#### 4. 互通域管理

manager角色可在前端界面设置互通域，指定某些Client可以相互通信。设置后，互通域保存在mongo数据库中，并且通知所有Server同步互通域，通过版本号管理。

![互通域管理](./PrjArchSrc/ManageIntraDomain.png)

#### 5. Client消息交互

Client先通过CommMngr api，获取当前最佳的Server地址进行连接，并通过ssl/tls协议进行身份验证和信息加密。 连接后，同样通过公私钥对，在Server上进行注册。 注册完成后即可对互通域内的所有Client进行通信。

![消息交互](./PrjArchSrc/Communication.png)

#### 6. Server监控以及远程控制、负载均衡

Server在启动后，定期向redis中写入监控数据例如cpu、内存使用情况，负载情况，由CommMngr进行远程管理，通过enable/disable来开启/关闭server转发服务。

![Server管理](./PrjArchSrc/ServerManager.png)

### 安全性说明

考虑到现在国家正在大力推进GmSSL，所以本项目除了SSL/TLS的安全服务都使用GmSSL算法库而非OpenSSL

#### 1. CommMngr、Server之间的安全性

* CommMngr如何验证Server的合法性？

  * 在server通过合法注册的partner注册后，Server应当生成SM2密钥对

  * CommMngr留存公钥，以作为该id的身份标识
  * 在Server连接上CommMngr、建立SSL连接之后，要求其对发出的Enc(随机数，PubKey)进行解密，并且将解密出的随机数附上签名返回给CommMngr，若随机数一致以及验签成功，才会继续其他业务的处理

* Server如何验证CommMngr的合法性？

  * partner注册后，可以通过在Server预置CommMngr SSL/TLS证书，在建立SSL连接时指定该证书，以验证CommMngr

* 消息交互过程：

  * 使用SSL信源加密，保证安全性

  综上，CommMngr、Server之间的安全性，使用SM2算法、SSL/TLS协议提供身份认证、授权、加密服务，保证数据完整性、非否认性。

#### 2. CommMngr、Client之间的安全性

* Client如何验证CommMngr的合法性？

  * Client使用https服务，保证CommMngr的合法性

* CommMngr如何验证Client的合法性？

  * Client发起https请求时，必须附带id及对当前时间戳的SM2私钥加密

* 消息交互过程：

  * 仅一次交互，通过SM2算法保证安全性

  综上，CommMngr、Client之间的安全性，使用SM2算法、SSL/TLS协议提供身份认证、授权、加密服务，并且通过时间戳来防止重放攻击

#### 3. Server、Client之间的安全性

* Client、Server如何验证双方的合法性

  * 前置条件：Server在CommMngr注册后，会获取当前所有的Client的id和公钥（可以增量更新，防止每次数据量过大）。Client则是启动后，可以通过向CommMngr发起请求，获取当前Server的最佳可用地址及其公钥。
* 注册过程：Client先发起注册请求，Server收到后，发出时间戳和随机数，通过Client公钥加密，Client解密后，将此二者返回，并且也生成自己的时间戳和随机数，使用Server公钥加密，发送给Server，要求其解密后返回。此过程验证完成后，Cient、Server正式注册，再进行相关业务处理。
  * 消息转发过程：可选SM4预置密钥/协商密钥进行消息信源加密，保证转发过程中的安全性。
* 而在Client-Client之间，则属于信源+信道加密

### 性能说明

* Server和Client、CommMngr和Server之间的通信使用msgpack编码，可以有效降低网络传输数据大小。
* Server和Client之间的加密传输通过SM4加密，性能略逊于AES，优于3DES，但内存需求通常低于此二者，可以更好地在低端设备上运行。若引入支持SM4的硬件提供加密服务/快速软件实现技术，加密性能将更上一筹。

## 构建流程

本仓库使用cmake构建，适配类unix系统，windows系统正在计划适配Client，若有某些系统不适配请联系作者。

版本信息：

cmake version 3.22.1 

gcc version 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04) 

java version "17.0.6" 2023-01-17 LTS

Apache Maven 3.6.3

@vue/cli 5.0.8

### 1. 克隆代码仓

git clone --recursive https://github.com/Montis132/MontComm.git (--recursive 克隆所有子模块的代码仓)

### 2. 构建

```sh
cd MontComm

./build.sh -a
```

### 3. （选读）单独模块构建

#### Ⅰ 三方仓构建

在工程目录执行./build -t ，将会自动进入third_party，执行third_party_build_all.sh脚本，构建gmssl()、libevent库（优先构建静态库）

三方库依赖以及其版本：

> [submodule "third_party/GmSSL"]
>
> ​	path = third_party/GmSSL
>
> ​	url = https://github.com/guanzhi/GmSSL.git (commit:6de0e022)
>
> [submodule "third_party/GmSSL-Java"]
>
> ​	path = third_party/GmSSL-Java
>
> ​	url = https://github.com/GmSSL/GmSSL-Java.git
>
> [submodule "third_party/json"]
>
> ​	path = third_party/json
>
> ​	url = https://github.com/nlohmann/json.git (commit:199dea11)
>
> [submodule "third_party/libevent"]
>
> ​	path = third_party/libevent
>
> ​	url = https://github.com/libevent/libevent.git (commit:d655c06b3a6b0fe8cff900f293bf0e5aac6eb0a2 v3.1.1)
>
> [submodule "third_party/msgpack"]
>
> ​	path = third_party/msgpack
>
> ​	url = https://github.com/msgpack/msgpack.git (tag:cpp-0.5.6)


#### Ⅱ 构建utils代码仓：

在工程目录执行./build -u ，将会自动进入utils，执行 utils_build.sh脚本，编译所有.c文件，并且将此代码仓的文件编译为.a文件。此代码仓包含了一些c语言编写的模块功能，包括安全管理模块、健康检查模块、日志模块、内存管理模块、命令行模块、线程池模块、定时器模块、网络消息模块、作者自己编写的双向循环链表，以及一些常用的api。（相关的模块说明待开发者补充）

该目录内置unittest，如果要执行单元测试，执行如下操作：

```sh
sudo apt-get install libcurl4-openssl-dev // 下载curl 4 openssl

cd utils

rm build -rf && mkdir build && pushd build

cmake -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=ON ..

make && make test
```

#### Ⅲ 构建Server 、 Client 代码共享仓

在工程目录执行./build -S ，将会自动进入SCShare，执行 scshare_build.sh脚本，将此代码仓的文件编译为.a文件。该仓使用msgpack进行消息序列化。

#### Ⅳ 构建 Server-Client 消息共享仓 MSShare

在工程目录执行：

```sh
cd MSShare && ./msshare_build.sh
```

MSShare 包含了 Server 和 Client 之间消息通信的消息头定义（MSMsg.h），使用msgpack-c API进行序列化/反序列化。

#### Ⅴ 构建 Server、Client 服务

在工程目录执行./build -sc ，将会自动进入Server、Client，执行脚本，编译。生成可执行文件Server/src/build/Server、 Client/src/build/Client。

#### Ⅵ 构建 Client_Qt 图形界面客户端

Client_Qt 是基于Qt框架的Client图形界面客户端，使用qmake构建：

```sh
cd Client_Qt
qmake Client_Qt.pro
make
```

依赖：Qt5（Widgets、Network模块）、GmSSL、msgpack-c、libevent。

#### Ⅶ 构建 utils-jni Java JNI 桥接库

utils-jni 封装了 utils C 库的 Java JNI 接口，使用 Maven 构建：

```sh
cd utils-jni
export JAVA_HOME=/usr/lib/jvm/java-17-oracle
mvn compile && mvn package
```

生成 jar 包供 CommMngr 后端调用，实现 Java 层对 utils C 安全、日志等功能的使用。

# RoadMap

### CommMngr 前端

* 提供不同权限账号注册界面 ×
* 提供对应权限账号注册不同角色的界面×
* 为partner提供证书导出界面按钮×
* 为partner、user提供密钥生成界面按钮×
* 为manager提供Server监控界面×
* 为parnter提供Server互通域管理增删改查界面×
* 提供SM2公钥导入界面×

### CommMngr 后端

* 为Server提供SSL CommMngr Server服务 √
* api接口框架搭建√
* 为api提供权限控制×
* 为Client提供Server信息请求接口（openapi）×
* 添加互通域增删改查api×
* 提供SM2公钥导入api×

### 消息序列化（protobuf → msgpack）

* 移除protobuf依赖（proto文件、pb.cc/pb.h） √
* 引入msgpack-c子模块（tag cpp-0.5.6） √
* 以msgpack重写SCShare/MSShare消息头（SCMsg.h、MSMsg.h） √
* msgpack-c独立编译为静态库，三个项目依赖 √
* end-to-end测试msgpack序列化/反序列化 ×
* 添加msgpack单元测试 ×
* 评估并升级msgpack-c版本到header-only（≥ cpp-2.0） ×

### util 组件

util组件是作者用c语言开发的使用工具仓，旨在为上层应用提供便捷的接口。目前仅适配类unix系统

* msg模块，提供tcp server/client消息交互接口
  * 提供带前后缀的消息接口，包含较为详细的信息包括sessionid、type、vermagic、clientid等，支持在消息末尾添加签名 √
  * 提供快速传输的消息接口，消息头仅包含ContentLen，其他服务由上层调用在Content中自行实现√
  * 提供消息分片功能 ×
  * 对health模块提供健康管理接口 √
* mem模块，提供内存管理接口 √
  * 对外提供默认内存申请/释放接口√
  * 对外提供注册接口，对单独的申请者提供内存管理服务√
  * 提供内存泄漏检查接口√
  * 对health模块提供健康管理接口 √
* cmdline模块，提供守护进程前台管理接口 √
  * 为上层提供注册自定义管理类型的接口(Util_CmdExternalRegister) √
  * 对health模块提供健康管理接口 √
* log模块，提供日志接口 √
  * 为上层提供适配cpp的打印类名的接口√
  * 提供日志文件自动切割功能√
  * 解决运行过程中日志文件被删除的问题√
  * 对health模块提供健康管理接口 √
* health模块，提供健康管理接口
  * 对外提供自定义的健康管理接口 √
* timer模块，提供定时任务接口
  * 将timer handle所有权转交外部√
  * 支持添加一次性和循环任务√
* threadpool模块，提供线程池接口
  * 对外提供定制化参数配置接口√
  * 对外提供实时修改参数配置接口√
  * 将threadpool实例所有权转交外部×
  * 对health模块提供健康管理接口 √
* crypt模块，提供安全服务接口 
  * 提供gmssl 密码学安全的随机数生成接口√
  * 提供gmssl sm2密钥生成/导入/导出接口√
  * 提供gmssl sm2签名/验签接口√
  * 提供gmssl sm2加密/解密接口√
  * 提供gmssl sm3 hash生成接口√
  * 提供gmssl sm3 hmac生成接口×
  * 提供gmssl sm4 ecb算法加密/解密接口×
  * 提供gmssl sm3 cbc算法加密/解密接口√
* 为每一个模块提供单元测试UnitTest，覆盖基本功能验证√
* 对外提供个性化初始化接口，包括自定义参数以及自定义模块√
* 为java提供utiljni服务（utils-jni） √

### Server

* 提供Client连接接口√
* 完善Client注册机制√
* 提供单进程多线程实例机制√
* 与CommMngr Server进行SSL通讯√
* 提供CommMngrServer自动重连逻辑√
* 定期向CommMngr上报监控数据√
* 定期flush长期未注册成功的Client×
* 完善CommMngr注册机制（50%）√
* 与Client之间引入SM4加解密√

### Client

* 提供Server自动重连逻辑√
* 增加Server断连自动重试状态机√
* 完善Server注册逻辑（50%）√
* 增加Server注册失败自动重试状态机√
* 引入SM4加解密√
* 从CommMngr获取Server相关信息×
* 为主流客户端提供图形化界面（via Qt，Client_Qt） √
