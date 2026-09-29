-- events.lua
-- Registered by nvbeam (mainwindow.cpp) with its RPC channel id as the
-- argument. Pushes cwd and current-file changes to the GUI (push model, no
-- polling): terminal follow-cwd, the window title and the switcher's
-- recently-used list all hang off these notifications, so nothing runs
-- while nvim is idle.
local chan = ...
local group = vim.api.nvim_create_augroup("nvbeam_events", { clear = true })

vim.api.nvim_create_autocmd("DirChanged", {
    group = group,
    callback = function()
        vim.rpcnotify(chan, "nvbeam_cwd", vim.fn.getcwd())
    end,
})

vim.api.nvim_create_autocmd("BufEnter", {
    group = group,
    callback = function()
        vim.rpcnotify(chan, "nvbeam_file", vim.api.nvim_buf_get_name(0))
    end,
})

-- Initial state: registration happens after startup, so the DirChanged and
-- BufEnter events for the initial cwd/buffer have already fired.
vim.rpcnotify(chan, "nvbeam_cwd", vim.fn.getcwd())
vim.rpcnotify(chan, "nvbeam_file", vim.api.nvim_buf_get_name(0))

return true
