-- minimap.lua
-- Registered once by neobeam (mainwindow.cpp); defines two functions the GUI
-- calls via tiny `return neobeam.*()` chunks instead of recompiling this file:
--   neobeam.minimap_view(): scroll/cursor state + changedtick. Cheap; polled
--     per flush. The GUI compares the tick and debounces the scan below, so
--     typing no longer triggers a treesitter parse per keystroke.
--   neobeam.minimap_scan(): full highlight scan of up to 2000 lines.
--     Expensive; called only after edits settle.
_G.neobeam = _G.neobeam or {}

-- Highlight-name -> color cache. Persists across scans; only a colorscheme
-- change can invalidate the mapping.
local fg_cache = {}
vim.api.nvim_create_autocmd("ColorScheme", {
    group = vim.api.nvim_create_augroup("neobeam_minimap", { clear = true }),
    callback = function() fg_cache = {} end,
})

local function get_fg(hl_name)
    local cached = fg_cache[hl_name]
    if cached ~= nil then return cached end
    local id = vim.fn.hlID(hl_name)
    if id == 0 then
        local base = hl_name:match("^@(.+)%..+$")
        if base then id = vim.fn.hlID("@" .. base) end
    end
    local fg = ""
    if id > 0 then
        fg = vim.fn.synIDattr(vim.fn.synIDtrans(id), "fg#") or ""
    end
    fg_cache[hl_name] = fg
    return fg
end

function neobeam.minimap_view()
    local buf = vim.api.nvim_get_current_buf()
    local win = vim.api.nvim_get_current_win()
    local info = vim.fn.getwininfo(win)[1]
    return {
        tick = vim.b[buf].changedtick,
        total = vim.api.nvim_buf_line_count(buf),
        top = info.topline,
        bot = info.botline,
        cursor = vim.api.nvim_win_get_cursor(win)[1],
    }
end

function neobeam.minimap_scan()
    local buf = vim.api.nvim_get_current_buf()
    local win = vim.api.nvim_get_current_win()
    local total = vim.api.nvim_buf_line_count(buf)
    local info = vim.fn.getwininfo(win)[1]
    local top = info.topline
    local bot = info.botline
    local cursor = vim.api.nvim_win_get_cursor(win)[1]
    local tick = vim.b[buf].changedtick

    local max_lines = math.min(total, 2000)
    local max_col = 80

    local lines = vim.api.nvim_buf_get_lines(buf, 0, max_lines, false)
    local hl_lines = {}
    for i = 1, max_lines do
        local len = math.min(#(lines[i] or ""), max_col)
        local row = {}
        for c = 1, len do row[c] = "" end
        hl_lines[i] = row
    end

    -- Treesitter highlight pass
    local ok, parser = pcall(vim.treesitter.get_parser, buf)
    if ok and parser then
        -- Force a synchronous, complete parse of the region we scan.
        -- get_parser() alone does NOT parse the buffer; for_each_tree/iter_captures
        -- would otherwise only see the viewport region that Neovim's async
        -- highlighter has parsed so far, leaving middle/bottom rows blank until a
        -- later redraw (the "flashing minimap" symptom). parse() called without an
        -- on_parse callback runs to completion synchronously. Bounding it to
        -- {0, max_lines} keeps injected-language scanning limited to the lines the
        -- minimap actually draws.
        pcall(function() parser:parse({ 0, max_lines }) end)

        pcall(function()
            parser:for_each_tree(function(tstree, ltree)
                if not tstree then return end
                local lang = ltree:lang()
                local qok, query = pcall(vim.treesitter.query.get, lang, "highlights")
                if not qok or not query then return end
                local root = tstree:root()
                for id, node in query:iter_captures(root, buf, 0, max_lines) do
                    local name = "@" .. query.captures[id] .. "." .. lang
                    local fg = get_fg(name)
                    if fg == "" then
                        fg = get_fg("@" .. query.captures[id])
                    end
                    if fg ~= "" then
                        local sr, sc, er, ec = node:range()
                        for r = sr, math.min(er, max_lines - 1) do
                            local row = hl_lines[r + 1]
                            if row then
                                local cs = (r == sr) and sc or 0
                                local ce = (r == er) and ec or #(lines[r + 1] or "")
                                ce = math.min(ce, max_col)
                                for c = cs + 1, ce do
                                    if row[c] == "" then row[c] = fg end
                                end
                            end
                        end
                    end
                end
            end)
        end)
    else
        -- Fallback: synID-based highlighting
        for i = 1, max_lines do
            local line = lines[i] or ""
            local len = math.min(#line, max_col)
            for c = 0, len - 1 do
                local id = vim.fn.synID(i, c + 1, true)
                if id > 0 then
                    local fg = vim.fn.synIDattr(vim.fn.synIDtrans(id), "fg#")
                    if fg and fg ~= "" then
                        hl_lines[i][c + 1] = fg
                    end
                end
            end
        end
    end

    -- `scanned` lets the GUI know how many lines this scan actually covered, so it
    -- can avoid caching this tick (and suppressing future refreshes) on a partial scan.
    return { tick = tick, lines = hl_lines, total = total, top = top, bot = bot, cursor = cursor, scanned = max_lines }
end

return true
