local Players, HttpService, WebSocketService, CorePackages = game:GetService("Players"), game:GetService("HttpService"), game:GetService("WebSocketService"), game:GetService("CorePackages")
local VirtualInputManager, RobloxReplicatedStorage = Instance.new("VirtualInputManager"), game:GetService("RobloxReplicatedStorage")

local ExecutorContainer, ScriptContainer, ObjectContainer = Instance.new("Folder"), Instance.new("Folder"), Instance.new("Folder")
ExecutorContainer.Name = "Executor"
ExecutorContainer.Parent = RobloxReplicatedStorage

ScriptContainer.Name = "Scripts"
ScriptContainer.Parent = ExecutorContainer

ObjectContainer.Name = "Objects"
ObjectContainer.Parent = ExecutorContainer


local PROCESS_ID, VERSION = %PROCESS_ID%, %VERSION%
local Executor, Bridge, Utils = {}, {}, {}
local user_agent = "Executor/"..VERSION
local client = WebSocketService:CreateClient("ws://localhost:9001")



function Utils:GetRandomModule()
	local children = CorePackages.Packages:GetChildren()
	local module

	while not module or module.ClassName ~= "ModuleScript" do
		module = children[math.random(#children)]
	end

	local clone = module:Clone()
	clone.Name = HttpService:GenerateGUID(false)
	clone.Parent = ScriptContainer

	return clone
end

function Utils:MergeTable(t1, t2)
	for i, v in pairs(t2) do
		t1[i] = v
	end
	return t1
end

function Utils:HttpGet(url, return_raw)
	assert(type(url) == "string", "invalid argument #1 to 'HttpGet' (string expected, got " .. type(url) .. ") ", 2)
	return_raw = return_raw or true
	local response = Executor.request({
		Url = url,
		Method = "GET",
	})

	if return_raw then
		return response.Body
	end

	return HttpService:JSONDecode(response.Body)
end



--// Bridge
Bridge.on_going_requests = {}

function Bridge:Send(data)
	if type(data) == "string" then
		data = HttpService:JSONDecode(data)
	end

	if not data["pid"] then
		data["pid"] = PROCESS_ID
	end

	client:Send(HttpService:JSONEncode(data))

end

function Bridge:SendAndReceive(data, timeout)
	timeout = timeout or 5

	local id = HttpService:GenerateGUID(false)
	data.id = id

	local bindable_event = Instance.new("BindableEvent")
	local response_data

    local connection
    connection = bindable_event.Event:Connect(function(response)
		response_data = response
        connection:Disconnect()
    end)

	self.on_going_requests[id] = bindable_event

	self:Send(data)

	local start_time = tick()
    while not response_data do
        if tick() - start_time > timeout then
            break
        end
		task.wait(0.1)
    end

	self.on_going_requests[id] = nil
    connection:Disconnect()
	bindable_event:Destroy()

	if not response_data then error("[Executor] timeout waiting for response " .. id, 2) end

    return response_data
end

function Bridge:IsCompilable(source)
	
	local response = self:SendAndReceive({
		["action"] = "is_compilable",
		["source"] = source
	})

	if not response["success"] then
		return false, response["message"]
	end

	return true
end

function Bridge:Loadstring(chunk, chunk_name)
	local module = Utils:GetRandomModule()

	local response =self:SendAndReceive({
		["action"] = "loadstring",
		["chunk"] = chunk,
		["chunk_name"] = chunk_name,
		["script_name"] = module.Name
	})


	if not response["success"] then
		return nil, response["message"]
	end

	local func = require(module)

	if debug.info(func, "n") ~= chunk_name then
		module:Destroy()
		return nil, "function does not match"
	end

	module.Parent = nil
	return func
end

function Bridge:Request(options)
	local response = self:SendAndReceive({
		["action"] = "request",
		["url"] = options.Url,
		["method"] = options.Method,
		["headers"] = options.Headers,
		["body"] = options.Body
	})

	if not response["success"] then
		error(response["message"], 3)
	end

	return {
		Success = response["response"]["success"],
		StatusCode = response["response"]["status_code"],
		StatusMessage = response["response"]["status_message"],
		Headers = response["response"]["headers"],
		Body = response["response"]["body"]
	}
end
--\\


--// Executor
Executor.base64 = {}

function Executor.base64.encode(data)
    local b = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    return ((data:gsub('.', function(x) 
        local r,b='',x:byte()
        for i=8,1,-1 do r=r..(b%2^i-b%2^(i-1)>0 and '1' or '0') end
        return r;
    end)..'0000'):gsub('%d%d%d?%d?%d?%d?', function(x)
        if (#x < 6) then return '' end
        local c=0
        for i=1,6 do c=c+(x:sub(i,i)=='1' and 2^(6-i) or 0) end
        return b:sub(c+1,c+1)
    end)..({ '', '==', '=' })[#data%3+1])
end
 
function Executor.base64.decode(data)
    local b = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    data = string.gsub(data, '[^'..b..'=]', '')
    return (data:gsub('.', function(x)
        if (x == '=') then return '' end
        local r,f='',(b:find(x)-1)
        for i=6,1,-1 do r=r..(f%2^i-f%2^(i-1)>0 and '1' or '0') end
        return r;
    end):gsub('%d%d%d?%d?%d?%d?%d?%d?', function(x)
        if (#x ~= 8) then return '' end
        local c=0
        for i=1,8 do c=c+(x:sub(i,i)=='1' and 2^(8-i) or 0) end
        return string.char(c)
    end))
end

function Executor.getgenv()
	return Executor
end
 
function Executor.request(options)
	assert(type(options) == "table", "invalid argument #1 to 'request' (table expected, got " .. type(options) .. ") ", 2)
	assert(type(options.Url) == "string", "invalid option 'Url' for argument #1 to 'request' (string expected, got " .. type(options.Url) .. ") ", 2)
	options.Method = options.Method or "GET"
	options.Method = options.Method:upper()
	assert(table.find({"GET", "POST", "PUT", "PATCH", "DELETE"}, options.Method), "invalid option 'Method' for argument #1 to 'request' (a valid http method expected, got '" .. options.Method .. "') ", 2)
	assert(not (options.Method == "GET" and options.Body), "invalid option 'Body' for argument #1 to 'request' (current method is GET but option 'Body' was used)", 2)
	if table.find({"POST", "PUT", "PATCH"}, options.Method) then
		assert(options.Body, "invalid option 'Body' for argument #1 to 'request' (current method is " .. options.Method .. " but option 'Body' was not provided)", 2)
	end
	if options.Body then
		assert(type(options.Body) == "string", "invalid option 'Body' for argument #1 to 'request' (string expected, got " .. type(options.Body) .. ") ", 2)
	end
	options.Headers = options.Headers or {}
	if options.Headers then assert(type(options.Headers) == "table", "invalid option 'Headers' for argument #1 to 'request' (table expected, got " .. type(options.Url) .. ") ", 2) end
	options.Headers["User-Agent"] = options.Headers["User-Agent"] or user_agent

	return Bridge:Request(options)
end


Executor.game = {}
setmetatable(Executor.game, {
	__index = function(self, index)
		if index == "HttpGet" or index == "HttpGetAsync" then
			return function(self, ...)
				return Utils:HttpGet(...)
			end
		end

		if type(game[index]) == "function" then
			return function(self, ...)
				return game[index](game, ...)
			end
		end

		return game[index]
	end,

	__tostring = function(self)
		return game.Name
	end,

	__metatable = getmetatable(game)
})

function Executor.loadstring(chunk, chunk_name)
	assert(type(chunk) == "string", "invalid argument #1 to 'loadstring' (string expected, got " .. type(chunk) .. ") ", 2)
	chunk_name = chunk_name or "loadstring"
	assert(type(chunk_name) == "string", "invalid argument #2 to 'loadstring' (string expected, got " .. type(chunk_name) .. ") ", 2)
	chunk_name = chunk_name:gsub("[^%a_]", "")


	local compile_success, compile_error = Bridge:IsCompilable(chunk)
	if not compile_success then
		return nil, chunk_name .. tostring(compile_error)
	end

	local func, loadstring_error = Bridge:Loadstring(chunk, chunk_name)

	if not func then
		return nil, loadstring_error
	end

	setfenv(func, getfenv(debug.info(2, 'f')))

	return func
end

function Executor.identifyexecutor()
	return "Executor", VERSION
end

function Executor.getscriptbytecode(script)
	assert(type(script) == "Instance", "invalid argument #1 to 'getscriptbytecode' (Instance expected, got " .. type(chunk) .. ") ", 2)

	if script.ClassName ~= "ModuleScript" and script.ClassName ~= "LocalScript" then
		error(script.Name .. " is not a LocalScript or a ModuleScript", 2)
	end
	
	
	local response = Bridge:SendAndReceive({
		["action"] = "get_script_bytecode",
		["path"] = script:GetFullName()
	})

	if not response["success"] then
		error(response["message"], 2)
	end

	return Executor.base64.decode(response["bytecode"])
end
--\\


client.MessageReceived:Connect(function(data)
	local success, data = pcall(function()
		return HttpService:JSONDecode(data)
	end)
	if not success then return end

	local id = data["id"]
	local _type = data["type"]


	if id and _type == "response" then
		return Bridge.on_going_requests[id]:Fire(data)
	end

	local action = data["action"]
	if not action then return end

	if action == "execute" then
		local response = {}
		response["type"] = "response"
		response["success"] = false

		if id then response["id"] = id end

		if not data["source"] then
			response["message"] = "Missing keys: source"
			return Bridge:Send(response)
		end

		local func, err = Bridge:Loadstring(data["source"], "Executor")

		if not func then
			response["message"] = err
			return Bridge:Send(response)
		end

		setfenv(func, Utils:MergeTable(getfenv(func), Executor))

		task.spawn(func)

		response["success"] = true
		return Bridge:Send(response)
	end

end)

client.Closed:Connect(function()
	warn("Websocket server has been closed, execution no longer works")
end)

local is_server_ready = false
client.Opened:Connect(function()
	is_server_ready = true
end)
while not is_server_ready do
	task.wait(0.1)
end

Bridge:Send({
	["action"] = "initialize",
})

print("[Executor] Injected successfully")