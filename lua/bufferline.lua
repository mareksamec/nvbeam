-- bufferline.lua
-- Registered once by nvbeam (mainwindow.cpp). The bufferline timer then calls
-- the tiny chunk `return nvbeam.bufferline()` instead of resending and
-- recompiling this file on every poll.
_G.nvbeam = _G.nvbeam or {}

function nvbeam.bufferline()
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
end

-- Close a list of buffers in one round trip. Modified buffers are only
-- deleted when `force` is set; otherwise they are left open and their names
-- are returned so the GUI can ask before discarding the changes.
function nvbeam.close_buffers(ids, force)
    local skipped = {}
    for _, b in ipairs(ids) do
        if vim.api.nvim_buf_is_valid(b) then
            if vim.bo[b].modified and not force then
                local name = vim.api.nvim_buf_get_name(b)
                table.insert(skipped, {id = b, name = name ~= "" and name or "[No Name]"})
            else
                pcall(vim.api.nvim_buf_delete, b, {force = force == true})
            end
        end
    end
    return skipped
end

return true
