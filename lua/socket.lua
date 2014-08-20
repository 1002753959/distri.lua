local Sche = require "lua/sche"
local Que  = require "lua/queue"
local socket = {}

local function sock_init(sock)
	sock.closing = false
	sock.errno = 0
	return sock
end

function socket:new(domain,type,protocal)
  local o = {}
  self.__index = self      
  setmetatable(o,self)
  o.luasocket = CSocket.new1(o,domain,type,protocal)
  if not o.luasocket then
		return nil
  end
  return sock_init(o)   
end

function socket:new2(sock)
  local o = {}
  self.__index = self          
  setmetatable(o, self)
  o.luasocket = CSocket.new2(o,sock)
  return sock_init(o)
end

function socket:close()
	print("socket:close")
	if not self.closing then
		self.closing = true	
		CSocket.close(self.luasocket)
		self.luasocket = nil
	end
end

local function on_new_conn(self,sock)
	self.new_conn:push({sock})	
	local co = self.block_onaccept:pop()
	if co then
		Sche.WakeUp(co)--Schedule(co)
	end
end

function socket:listen(ip,port)
	if self.closing then
		return "socket close"
	end
	if self.block_onaccept or self.new_conn then
		return "already listening"
	end
	self.block_onaccept = Que.Queue()
	self.new_conn = Que.Queue()
	self.__on_new_connection = on_new_conn
	return CSocket.listen(self.luasocket,ip,port)
end

local function on_disconnected(self,errno)
	self.errno = errno
	self.closing = true
	local co
	while self.block_noaccept do
		co = self.block_onaccept:pop()
		if co then
			Sche.WakeUp(co)--Schedule(co)
		else
			self.block_noaccept = nil
		end
	end

	while true do
		co = self.block_recv:pop()
		if co then
			Sche.WakeUp(co)--Schedule(co) 
		else
			break
		end
	end
	
	if self.connect_co then
		Sche.WakeUp(self.connect_co)--Schedule(self.connect_co)
	end	
end

local function on_packet(self,packet)
	self.packet:push({packet})
	local co = self.block_recv:front()
	if co then
	    self.timeout = nil
		--Sche.Schedule(co)
		Sche.WakeUp(co)		
	end
end

local function establish(sock,max_packet_size)
	sock.isestablish = true
	sock.__on_packet = on_packet
	sock.__on_disconnected = on_disconnected
	sock.block_recv = Que.Queue()	
	CSocket.establish(sock.luasocket,max_packet_size)
	sock.packet = Que.Queue()	
end


function socket:accept(max_packet_size)
	if self.closing then
		return nil,"socket close"
	end

	if not self.block_onaccept or not self.new_conn then
		return nil,"invaild socket"
	else	
		while true do
			local s = self.new_conn:pop()
			if s then
			    s = s[1]
				local sock = socket:new2(s)
				establish(sock,max_packet_size or 65535)
				return sock,nil
			else
				local co = Sche.Running()
				if not co then
					return nil,"accept must be call in a coroutine context"
				end
				self.block_onaccept:push(co)
				Sche.Block()
				if  self.closing then
					return nil,"socket close" --socket被关闭
				end				
			end
		end
	end
end

local function cb_connect(self,s,err)
	if not s or err ~= 0 then
		self.err = err
	else
		self.luasocket = CSocket.new2(self,s)
	end
	local co = self.connect_co
	self.connect_co = nil
	Sche.WakeUp(co)--Schedule(co)	
end

function socket:connect(ip,port,max_packet_size)
	local ret = CSocket.connect(self.luasocket,ip,port)
	if ret then
		return ret
	else
		self.connect_co = Sche.Running()
		if not self.connect_co then
			return "connect must be call in a coroutine context"
		end
		self.___cb_connect = cb_connect
		Sche.Block()
		if self.closing then
			return "socket close" 
		elseif self.err then
			return self.err
		else
			establish(self,max_packet_size or 65535)	
			return nil
		end				
	end
end


function socket:recv()
	if self.closing then
		return nil,"socket close"	
	elseif not self.isestablish then
		return nil,"invaild socket"
	end
	while true do
		local packet = self.packet:pop()
		if packet then
			return packet[1],nil
		end		
		local co = Sche.Running()
		if not co then
			return nil,"recv must be call in a coroutine context"
		end		
		self.block_recv:push(co)		
		if timeout then
			self.timeout = timeout
		end
		Sche.Block(timeout)
		if self.timeout then
		    self.timeout = nil
		    self.block_recv:remove(co)
			return nil,"recv timeout"
		else
			self.block_recv:pop()
			if self.closing then
				return nil,self.errno
			end			
		end
	end
end

function socket:send(packet)
	if self.closing then
		return "socket close"
	end
	if not self.luasocket then
		return "invaild socket"
	end
	return CSocket.send(self.luasocket,packet)
end

return {
	new = function (domain,type,protocal) return socket:new(domain,type,protocal) end
}

