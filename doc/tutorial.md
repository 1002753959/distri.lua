#distri.lua使用引导

目录

* [概述](#概述)
* [安装distri.lua](#安装distri.lua)
* [事件处理模型](#事件处理模型)
* [协程的使用](#协程的使用)
* [网络编程](#网络编程)
* [RPC](#RPC)
* [redis接口](#redis接口)
* [定时器](#定时器)
* [通过C模块扩展](#通过C模块扩展)
* [一个小型手游服务端示例](#一个小型手游服务端示例)

###<span id="概述">概述</span>

distri.lua是一个轻量级的lua网络应用框架,其主要设计目标是使用lua语言快速开发小型的分布式系统,网络游戏服务端程序,web应用等.distri.lua的特点:

* 快速高效的网络模型
* 基于lua协程的并发处理
* 提供了同步的RPC调用接口
* 单线程(除日志线程)
* 内置同步的redis访问接口
* http协议处理
* 内置libcurl接口

###<span id="安装distri.lua">安装distri.lua</span>
