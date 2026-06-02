local bufs = {}
local cur = vim.api.nvim_get_current_buf()
for _, b in ipairs(vim.api.nvim_list_bufs()) do
    if vim.bo[b].buflisted then
        table.insert(bufs, {
            id = b,
            name = vim.api.nvim_buf_get_name(b),
            modified = vim.bo[b].modified
        })
    end
end
return {bufs = bufs, current = cur}
