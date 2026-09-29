-- Registered by nvbeam (mainwindow.cpp) with its RPC channel id as the
-- argument. Pushes a notification whenever the git state may have changed so
-- the Qt side re-runs `git status` (push model, no polling). Changes made
-- outside nvim (commits in the embedded terminal) are covered separately by
-- a file watcher on .git/HEAD and .git/index (see src/gitstatus.cpp).
local chan = ...
vim.api.nvim_create_autocmd({ "BufWritePost", "DirChanged" }, {
    group = vim.api.nvim_create_augroup("nvbeam_git", { clear = true }),
    callback = function()
        vim.rpcnotify(chan, "nvbeam_git_event")
    end,
})
return true
