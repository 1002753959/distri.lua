local sche = require "lua/sche"
local Packet = require "lua/packet"
local TcpServer = require "lua/tcpserver"
local Connection = require "lua/connection"
local App = require "lua/application"
local RPC = require "lua/rpc"

local count = 0
local rpcserver = App.Application()

local function on_disconnected(conn,err)
	print("conn disconnected:" .. err)
	conn:close()	
end

rpcserver:run(function ()
	TcpServer.Listen("127.0.0.1",8000,function (client)
			print("on new client")
			local  conn = Connection.Connection(client,Packet.RPacketDecoder(65535))		
			RPC.RPCServer(conn,"Plus",function (a,b)
				count = count + 1 
				return a+b 
			end)
			rpcserver:add(conn,nil,on_disconnected)		
	end)
end)

print("server start on 127.0.0.1:8000")

local tick = GetSysTick()
local now = GetSysTick()
while true do 
	now = GetSysTick()
	if now - tick >= 1000 then
		print(count*1000/(now-tick) .. " " .. now-tick)
		tick = now
		count = 0
	end
	sche.Sleep(10)
end

