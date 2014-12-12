local Sche = require "lua.sche"
local RPC = require "lua.rpc"
local Timer = require "lua.timer"
local application = {}

function application:new()
  local o = {}   
  setmetatable(o, self)
  self.__index = self
  o.sockets = {}
  o.running = false
  o._RPCService = {}
  return o
end

local CMD_PING = 0xABABCBCB

local max_recver_per_socket = 10

local function recver(app,socket)
	while app.running do
		local rpk,err = socket:Recv()
		if err then
			socket:Close()
			break
		end
		if rpk then
			if socket.check_recvtimeout then 
				socket.lastrecv = C.GetSysTick()
			end
			local cmd = rpk:Peek_uint32()
			if cmd and cmd == RPC.CMD_RPC_CALL or cmd == RPC.CMD_RPC_RESP then
				--如果是rpc消息，执行rpc处理
				if cmd == RPC.CMD_RPC_CALL then
					rpk:Read_uint32()
					RPC.ProcessCall(app,socket,rpk)
				elseif cmd == RPC.CMD_RPC_RESP then
					rpk:Read_uint32()
					RPC.ProcessResponse(socket,rpk)
				end
			elseif cmd and cmd == CMD_PING then
				socket:Send(CPacket.NewWPacket(rpk))		
			elseif socket.process_packet then
				local ret,err = pcall(socket.process_packet,socket,rpk)
				if not ret then
					CLog.SysLog(CLog.LOG_ERROR,"application process_packet error:" .. err)
				end
			end
		end		
	end
	socket.recver_count = socket.recver_count - 1
end


local heart_beat_timer = Timer.New("runImmediate")

function application:Add(socket,on_packet,on_disconnected,recvtimeout,pinginterval)
	if not self.sockets[socket] then
		self.sockets[socket] = socket
		--socket.application = self
		socket.recver_count = 0
		socket.process_packet = on_packet
		local app = self
		socket.on_disconnected = function (sock,errno) 
						if on_disconnected then
							on_disconnected(sock,errno)
						end
						app.sockets[sock] = nil
					end
		socket.check_recvtimeout = recvtimeout
		local app = self
		--改变conn.sock.__on_packet的行为
		socket.__on_packet = function (socket,packet)
			socket.packet:Push({packet})
			local co = socket.block_recv:Front()
			if co then
				co = co[1]
				socket.timeout = nil
				Sche.WakeUp(co)		
			elseif app.running and socket.recver_count < max_recver_per_socket then
				socket.recver_count = socket.recver_count + 1
				Sche.SpawnAndRun(recver,app,socket)
			end
		end
		
		if recvtimeout then
			socket.lastrecv = C.GetSysTick()
			heart_beat_timer:Register(
				function ()
					if socket.luasocket then
						if C.GetSysTick() > socket.lastrecv + recvtimeout then
							socket:Close()
							return "stoptimer"
						end
					else
						return "stoptimer"
					end
				end,1000)
		end
		
		if pinginterval then
			heart_beat_timer:Register(
				function ()
					if socket.luasocket then
						local wpk = CPacket.NewWPacket(64)
						wpk:Write_uint32(CMD_PING)
						socket:Send(wpk)
					else
						return "stoptimer"
					end
				end,pinginterval)		
		end	
	end
	return self
end

function application:RPCService(name,func)
	self._RPCService[name] = func
end

function application:Run(start_fun)
	if start_fun then
		start_fun()
	end
	self.running = true
end

function application:Stop()
	self.running = false
	for k,v in pairs(self.sockets) do
		v:Close()
	end
end

local function SetMaxRecverPerSocket(count)
	if not count or count == 0 then
		count = 10
	end
	max_recver_per_socket = count
end

return {
	New =  function () return application:new() end,
	SetMaxRecverPerSocket = SetMaxRecverPerSocket
}
