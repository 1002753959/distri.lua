local LightProcess = require "lua/light_process"
local scheduler =
{
    pending_add,  --�ȴ���ӵ���б��е�coObject
    timer,
    CoroCount,
    current_lp
}

local function scheduler:new(o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

local function scheduler:init()
    self.m_timer = timer:new()
    self.pending_add = {}
    self.current_lp = nil
    self.CoroCount = 0
end

--��ӵ���б���
local function scheduler:Add2Active(lprocess)
    if lprocess.status == "actived" then
        return
    end
    lprocess.status = "actived"
    table.insert(self.pending_add,lprocess)
end

local function scheduler:Block(ms)
    local lprocess = self.current_lp
    if ms and ms > 0 then
        local nowtick = GetSysTick()
        lprocess.timeout = nowtick + ms
        if lprocess.index == 0 then
            self.m_timer:Insert(lprocess)
        else
            self.m_timer:Change(lprocess)
        end
    end
    lprocess.status = "block"
    coroutine.yield(lprocess.croutine)
    --�������ˣ�������г�ʱ�ڶ����У�������Ҫ������ṹ�ŵ�����ͷ��������ɾ��
    if lprocess.index ~= 0 then
        lprocess.timeout = 0		
        self.m_timer:Change(lprocess)
        self.m_timer:PopMin()
    end
end


--˯��ms
local function scheduler:Sleep(ms)
    local lprocess = self.current_lp
    if ms and ms > 0 then
        lprocess.timeout = GetSysTick() + ms
        if lprocess.index == 0 then
            self.m_timer:Insert(lprocess)
        else
            self.m_timer:Change(lprocess)
        end
        lprocess.status = "sleep"
    else
        lprocess.status = "yield"
    end
    coroutine.yield(lprocess.croutine)
end

--��ʱ�ͷ�ִ��Ȩ
local function scheduler:Yield()
    self:Sleep(0)
end


--������ѭ��
local function scheduler:Schedule()
    local runlist = {}
    --��pending_add������coObject��ӵ���б���
    for k,v in pairs(self.pending_add) do
        table.insert(runlist,v)
    end

    self.pending_add = {}
    local now_tick = GetSysTick()
    for k,v in pairs(runlist) do
        self.current_lp = v
        coroutine.resume(v.croutine,v)
        self.current_lp = nil
        if v.status == "yield" then
            self:Add2Active(v)
        elseif v.status == "dead" then
			print("a light process dead")
		end
    end
    runlist = {}
    --������û��timeout���˳�
    local now = GetSysTick()
    while self.m_timer:Min() ~=0 and self.m_timer:Min() <= now do
        local lprocess = self.m_timer:PopMin()
        if lprocess.status == "block" or lprocess.status == "sleep" then
            self:Add2Active(lprocess)
        end
    end
    
    return #self.pending_add
end

local function scheduler:WakeUp(lprocess)
    self:Add2Active(lprocess)
end

local global_sc = scheduler:new()
local global_sc:init()

local function Yield()
    global_sc:Yield()
end

local function Sleep(ms)
    global_sc:Yield(ms)
end

local function Block(ms)
    global_sc:Block(ms)
end

local function WakeUp(lprocess)
   global_sc:WakeUp(lprocess)
end

local function GetCurrentLightProcess()
    return global_sc.current_lp
end

local function GetLightProcessByIdentity(identity)

end

local function Schedule()
	global_sc:Schedule()
end

local function lp_start_fun(lp)
    print("lp_start_fun")
	global_sc.CoroCount = global_sc.CoroCount + 1
	lp.start_func(lp.ud)
	lp.status = "dead"
	lp.ud = nil
	global_sc.CoroCount = global_sc.CoroCount - 1
	print("end lp_start_fun")
end

local function Spawn(func,ud)
    print("node_spwan")
    local lprocess = LightProcess.NewLightProcess(lp_start_fun,ud,func)
    global_sc:Add2Active(lprocess)
end

return {
		Spawn = Spawn,
		Yield = Yield,
		Sleep = Sleep,
		Block = Block,
		WakeUp = WakeUp,
		GetCurrentLightProcess = GetCurrentLightProcess,
		Schedule = Schedule
}

