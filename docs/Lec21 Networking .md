# 计算机网络概述
今天我想讨论一下Networking以及它与操作系统的关联。今天这节课的很多内容都与最后一个lab，也就是构建一个网卡驱动相关。在这节课，我们首先会大概看一下操作系统中网络相关的软件会有什么样的结构，之后我们会讨论今天的论文Livelock。Livelock展示了在设计网络协议栈时可能会出现的有趣的陷阱。

## 基本的网络连接场景
首先，让我通过画图来描述一下基本的网络场景。网络连接了不同的主机，这里的连接有两种方式：

1. 局域网（Local Area Network）:  相近的主机连接在同一个网络中。
例如有一个以太网设备，可能是交换机或者单纯的线缆，然后有一些主机连接到了这个以太网设备。这里的主机可能是笔记本电脑，服务器或者路由器。在设计网络相关软件的时候，通常会忽略直接连接了主机的网络设备。这里的网络设备可能只是一根线缆（几十年前就是通过线缆连接主机）；也可能是一个以太网交换机；也可能是wifi无线局域网设备（主机通过射频链路与网络设备相连），但是不管是哪种设备，这种直接连接的设备会在网络协议栈的底层被屏蔽掉。

每个主机上会有不同的应用程序，或许其中一个主机有网络浏览器，另一个主机有HTTP server，它们需要通过这个局域网来相互通信。

2. Internet
一个局域网的大小是有极限的。局域网（Local Area Network）通常简称为LAN。一个局域网需要能让其中的主机都能收到彼此发送的packet。有时，主机需要广播packet到局域网中的所有主机。当局域网中只有25甚至100个主机时，是没有问题的。但是你不能构建一个多于几百个主机的局域网。

所以为了解决这个问题，大型网络是这样构建的。首先有多个独立的局域网，假设其中一个局域网是MIT，另一个局域网是Harvard，还有一个很远的局域网是Stanford，在这些局域网之间会有一些设备将它们连接在一起，这些设备通常是路由器Router。其中一个Router接入到了MIT的局域网，同时也接入到了Harvard的局域网。

路由器是组成互联网的核心，路由器之间的链路，最终将多个局域网连接在了一起。

现在MIT有一个主机需要与Stanford的一个主机通信，他们之间需要经过一系列的路由器，路由器之间的转发称为 `Routing` 。所以我们需要有一种方法让MIT的主机能够寻址到Stanford的主机，并且我们需要让连接了MIT的路由器能够在收到来自MIT的主机的packet的时候，能够知道这个packet是发送给Harvard的呢，还是发送给Stanford的。

## 协议

从网络协议的角度来说
- 局域网通信由以太网协议决定。
- 局域网之上的长距离网络通信由Internet Protocol协议决定。

以上就是网络的概述。

接下来我想介绍一下，在局域网和互联网上传递的packet有什么样的结构，之后再讨论在主机和路由器中的软件是如何处理这些packet。


# 二层网络: Ethernet
让我从最底层开始，我们先来看一下一个以太网packet的结构是什么。当两个主机非常靠近时，或许是通过相同的线缆连接，或许连接在同一个wifi网络，或许连接到同一个以太网交换机。当局域网中的两个主机彼此间要通信时，最底层的协议是以太网协议。

## Frame
你可以认为Host1通过以太网将 `Frame` 发送给 Host2。`Frame` 是以太网中用来描述 `packet` 的单词，本质上这就是两个主机在以太网上传输的一个个的数据Byte。以太网协议会在Frame中放入足够的信息让主机能够识别彼此，并且识别这是不是发送给自己的Frame。

### Header

```c
//
// useful networking headers
//

#define ETHADDR_LEN 6

// an Ethernet packet header (start of the packet).
struct eth {
  uint8  dhost[ETHADDR_LEN];
  uint8  shost[ETHADDR_LEN];
  uint16 type;
} __attribute__((packed));
```

每个以太网packet在最开始都有一个 `Header` ，其中包含了3个数据。Header之后才是 `payload` 数据。
Header 中的3个数据是：
- 目的以太网地址(48bit)
  每一个以太网地址都是48bit的数字，这个数字唯一识别了一个网卡。


- 源以太网地址(48bit)
  
- packet的类型(16bit)
  packet的类型会告诉接收端的主机该如何处理这个packet。 接收端主机侧更高层级的网络协议会按照packet的类型检查并处理以太网packet中的payload。




整个以太网packet，包括了48bit+48bit的以太网地址，16bit的类型，以及任意长度的payload这些都是通过线路传输。
除此之外，虽然对于软件来说是不可见的，但是在packet的开头还有被硬件识别的表明packet起始的数据（注，Preamble + SFD），在packet的结束位置还有几个bit表明packet的结束（注，FCS）。
packet的开头和结束的标志不会被系统内核所看到，其他的部分会从网卡送到系统内核。

如果你们查看了这门课程的最后一个lab，你们可以发现我们提供的代码里面包括了一些新的文件，其中包括了kernel/net.h，这个文件中包含了大量不同网络协议的packet header的定义。上图中的代码包含了以太网协议的定义。
我们提供的代码使用了这里结构体的定义来解析收到的以太网packet，进而获得目的地址和类型值（注，实际中只需要对收到的raw data指针强制类型转换成结构体指针就可以完成解析）。



学生提问：硬件用来识别以太网packet的开头和结束的标志是不是类似于lab中的End of Packets？

Robert教授：并不是的，EOP是帮助驱动和网卡之间通信的机制。这里的开头和结束的标志是在线缆中传输的电信号或者光信号，这些标志位通常在一个packet中是不可能出现的。以结束的FCS为例，它的值通常是packet header和payload的校验和，可以用来判断packet是否合法。

## 以太网地址 vs IP地址

### 以太网地址
有关以太网48bit地址，是为了给每一个制造出来的网卡分配一个唯一的ID，所以这里有大量的可用数字。
- 这里48bit地址中，前24bit表示的是制造商，每个网卡制造商都有自己唯一的编号，并且会出现在前24bit中。
- 后24bit是由网卡制造商提供的任意唯一数字，通常网卡制造商是递增的分配数字。
所以，如果你从一个网卡制造商买了一批网卡，每个网卡都会被写入属于自己的地址，并且如果你查看这些地址，你可以发现，这批网卡的高24bit是一样的，而低24bit极有可能是一些连续的数字。

### IP地址
虽然以太网地址是唯一的，但是出了局域网，它们对于定位目的主机的位置是没有帮助的。
如果网络通信的目的主机在同一个局域网，那么目的主机会监听发给自己的地址的packet。
但是如果网络通信发生在两个国家的主机之间，你需要使用一个不同的寻址方法，这就是IP地址的作用。


### tcpdump
在实际中，你可以使用tcpdump来查看以太网packet。这将会是lab的一部分。下图是tcpdump的一个输出：

tcpdump输出了很多信息，其中包括：
- 接收packet的时间
- 第一行的剩下部分是可读的packet的数据
- 接下来的3行是收到packet的16进制数

如果按照前面以太网header的格式，可以发现packet中：
- 0xffff ffff ffff: 前48bit是一个广播地址，广播地址是指packet需要发送给局域网中的所有主机。
- 0x5255 0a00 0202: 之后的48bit是发送主机的以太网地址，我们并不能从这个地址发现什么，实际上这个地址是运行在QEMU下的XV6生成的地址，所以地址中的前24bit并不是网卡制造商的编号，而是QEMU编造的地址。
- 0x0806 :          接下来的16bit是以太网packet的类型，对应的协议是ARP。
- 剩下的部分是ARP packet的payload。

# 二/三层地址转换 --- ARP

下一个与以太网通信相关的协议是ARP。
在以太网层面，每个主机都有一个以太网地址。但是为了能在互联网上通信，你需要有32bit的IP地址。

为什么需要IP地址呢？
因为IP地址有额外的含义。
- IP地址的高位bit包含了在整个互联网中，这个packet的目的地在哪。所以IP地址的高位bit对应的是网络号，虽然实际上要更复杂一些，但是你可以认为互联网上的每一个网络都有一个唯一的网络号。路由器会检查IP地址的高bit位，并决定将这个packet转发给互联网上的哪个路由器。

- IP地址的低bit位代表了在局域网中特定的主机。当一个经过互联网转发的packet到达了局域以太网，我们需要从32bit的IP地址，找到对应主机的48bit以太网地址。这里是通过一个动态解析协议完成的，也就是Address Resolution Protocol，ARP协议。

当一个packet到达路由器并且需要转发给同一个以太网中的另一个主机，或者一个主机将packet发送给同一个以太网中的另一个主机时，发送方首先会在局域网中广播一个ARP packet，来表示任何拥有了这个32bit的IP地址的主机，请将你的48bit以太网地址返回过来。如果相应的主机存在并且开机了，它会向发送方发送一个ARP response packet。

 ```c
// an ARP packet (comes after an Ethernet header).
struct arp {
  uint16 hrd; // format of hardware address
  uint16 pro; // format of protocol address
  uint8  hln; // length of hardware address
  uint8  pln; // length of protocol address
  uint16 op;  // operation

  char   sha[ETHADDR_LEN]; // sender hardware address
  uint32 sip;              // sender IP address
  char   tha[ETHADDR_LEN]; // target hardware address
  uint32 tip;              // target IP address
} __attribute__((packed));

#define ARP_HRD_ETHER 1 // Ethernet

enum {
  ARP_OP_REQUEST = 1, // requests hw addr given protocol addr
  ARP_OP_REPLY = 2,   // replies a hw addr given protocol addr
};

 ```