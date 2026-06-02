local mode = vim.api.nvim_get_mode().mode
local mode_map = {
    n = "NORMAL", i = "INSERT", v = "VISUAL", V = "V-LINE",
    ["\22"] = "V-BLOCK", c = "COMMAND", R = "REPLACE",
    t = "TERMINAL", s = "SELECT", S = "S-LINE",
}
local buf = vim.api.nvim_get_current_buf()
local name = vim.api.nvim_buf_get_name(buf)
if name == "" then name = "[No Name]" end
local pos = vim.api.nvim_win_get_cursor(0)
local encoding = vim.o.fileencoding ~= "" and vim.o.fileencoding or vim.o.encoding
local branch = ""
if vim.b.gitsigns_head then branch = " " .. vim.b.gitsigns_head end
local diag = ""
local e = #vim.diagnostic.get(buf, {severity = vim.diagnostic.severity.ERROR})
local w = #vim.diagnostic.get(buf, {severity = vim.diagnostic.severity.WARN})
if e > 0 or w > 0 then
    diag = "E:" .. e .. " W:" .. w
end
return {
    mode = mode_map[mode] or mode,
    file = name,
    pos = pos[1] .. ":" .. pos[2],
    encoding = encoding,
    branch = branch,
    diagnostics = diag,
}
