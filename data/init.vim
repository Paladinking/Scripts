let plug_location = stdpath('data') . '/site/autoload/plug.vim'
if !filereadable(plug_location)
    if has('win64')
        silent execute '!powershell -c "iwr -useb https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim | ni """' . plug_location . '""" -Force"'
    else
        silent execute '!curl -fLo ' . plug_location . ' --create-dirs https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim'
    endif
    exec "source" plug_location
    autocmd VimEnter * PlugInstall --sync | source $MYVIMRC | :q
endif

call plug#begin()
Plug 'neovim/nvim-lspconfig'

Plug 'nvim-lua/plenary.nvim'
Plug 'nvim-telescope/telescope.nvim', { 'branch': '0.1.x' }

Plug 'hrsh7th/cmp-nvim-lsp'
Plug 'hrsh7th/cmp-buffer'
Plug 'hrsh7th/cmp-path'
Plug 'hrsh7th/cmp-cmdline'
Plug 'hrsh7th/nvim-cmp'
"Plug 'sakhnik/nvim-gdb'

Plug 'hrsh7th/cmp-vsnip'
Plug 'hrsh7th/vim-vsnip'

Plug 'nvim-tree/nvim-web-devicons' " optional, for file icons
Plug 'nvim-tree/nvim-tree.lua'

"Plug 'mhinz/vim-signify'

Plug 'morhetz/gruvbox'
Plug 'jeetsukumaran/vim-indentwise'
"Plug 'romgrk/barbar.nvim'

Plug 'tpope/vim-fugitive'

Plug 'nvim-treesitter/nvim-treesitter', { 'branch': 'main'}

Plug 'sindrets/diffview.nvim'

Plug 'rebelot/kanagawa.nvim'

call plug#end()

if has('win64')
    language en_US
endif
colorscheme kanagawa

map [- <Plug>(IndentWisePreviousLesserIndent)

let bufferline = get(g:, 'bufferline', {})

set completeopt=menu,menuone,noselect

nnoremap <A-down> :m .+1<CR>==
nnoremap <A-up> :m .-2<CR>==
inoremap <A-down> <Esc>:m .+1<CR>==gi
inoremap <A-up> <Esc>:m .-2<CR>==gi
vnoremap <A-down> :m '>+1<CR>gv=gv
vnoremap <A-up> :m '<-2<CR>gv=gv

nnoremap <A-j> :m .+1<CR>==
nnoremap <A-k> :m .-2<CR>==
inoremap <A-j> <Esc>:m .+1<CR
inoremap <A-k> <Esc>:m .-2<CR>=
vnoremap <A-j> :m '>+1<CR>gv=
vnoremap <A-k> :m '<-2<CR>gv=gv

vnoremap <C-c> "*y

vnoremap <Bs> <Del>
vnoremap <C-x> <Del>

nnoremap <C-d> yyp
inoremap <C-d> <Esc>yyp<CR>==gi

nnoremap <C-q>  :let @/ = ""<CR>
nnoremap <C-t> :NvimTreeOpen<CR>

set shiftwidth=4 smarttab
set expandtab
set nomodeline
set number

let g:neoformat_basic_format_trim = 1
let g:neoformat_basic_format_retab = 1
let g:neoformat_basic_format_align = 1

hi link markdownError NormalFloat

"set grepprg=grep\ -nH -rIi
set grepprg=rg\ --vimgrep\ --no-heading\ --smart-case
set grepformat=%f:%l:%m
set grepformat+=%f:%l:%c:%m
command! -nargs=1 Search silent grep! <args> | copen
inoremap <C-F> <Esc>:Search 
vnoremap <C-F> :Search 
nnoremap <C-F> :Search 

vnoremap q <C-v>
nnoremap <A-v> <C-v>

nnoremap & <Nop>

autocmd BufReadPre *.asm let g:asmsyntax = "fasm"

set rulerformat=%l,%c\ (byte\ %o)
nnoremap dd "0dd
nnoremap p "0p
vnoremap d "0d
vnoremap p "0p

let mapleader = ","

nnoremap ff <cmd>Telescope find_files<cr>
nnoremap <leader>fr <cmd>Telescope resume<cr><Esc>
nnoremap <leader>fg <cmd>Telescope live_grep<cr>
nnoremap <leader>fb <cmd>Telescope buffers<cr>
nnoremap <leader>fh <cmd>Telescope help_tags<cr>
nnoremap <leader>fs <cmd>Telescope grep_string<cr>

lua <<EOF


vim.g.loaded_netrw = 1
vim.g.loaded_netrwPlugin = 1

-- set termguicolors to enable highlight groups
vim.opt.termguicolors = true

-- empty setup using defaults
require("nvim-tree").setup({
    view = {
        width = 40
    },
    filesystem_watchers = {
        enable = false,
        debounce_delay = 50,
        ignore_dirs = {},
    }
})

require("nvim-treesitter.configs").setup({
    highlight = {
        enable = true,
        additional_vim_regex_highlighting = { "python" }
    },
    indent = {
        enable = false
    }
})

local nvim_tree_events = require('nvim-tree.api').events

local cmp = require'cmp'

cmp.setup({
snippet = {
    -- REQUIRED - you must specify a snippet engine
    expand = function(args)
    vim.fn["vsnip#anonymous"](args.body) -- For `vsnip` users.
    end,
},
window = {
    -- completion = cmp.config.window.bordered(),
    documentation = cmp.config.window.bordered(),
},
mapping = cmp.mapping.preset.insert({
    ['<C-b>'] = cmp.mapping.scroll_docs(-4),
    ['<C-f>'] = cmp.mapping.scroll_docs(4),
    ['<C-Space>'] = cmp.mapping.complete(),
    ['<C-e>'] = cmp.mapping.abort(),
    ['<Tab>'] = cmp.mapping.select_next_item(),
    ['<S-Tab>'] = cmp.mapping.select_prev_item(),
    ['<CR>'] = cmp.mapping.confirm({ select = false }), -- Accept currently selected item. Set `select` to `false` to only confirm explicitly selected items.
}),
sources = cmp.config.sources({
    { name = 'nvim_lsp' },
    { name = 'vsnip' }, -- For vsnip users.
    }, {
        { name = 'buffer' },
    })
})

-- Set configuration for specific filetype.
cmp.setup.filetype('gitcommit', {
    sources = cmp.config.sources({
    { name = 'cmp_git' }, -- You can specify the `cmp_git` source if you were installed it.
    }, {
        { name = 'buffer' },
    })
    })

-- Use buffer source for `/` and `?` (if you enabled `native_menu`, this won't work anymore).
cmp.setup.cmdline({ '/', '?' }, {
    mapping = cmp.mapping.preset.cmdline(),
    sources = {
        { name = 'buffer' }
    }
    })

-- Use cmdline & path source for ':' (if you enabled `native_menu`, this won't work anymore).
cmp.setup.cmdline(':', {
    mapping = cmp.mapping.preset.cmdline(),
    sources = cmp.config.sources({
    { name = 'path' }
    }, {
        { name = 'cmdline' }
    })
    })

-- Set up lspconfig.
local capabilities = require('cmp_nvim_lsp').default_capabilities()

--require('lspconfig')['jedi_language_server'].setup {
--    capabilities = capabilities
--}

--require('lspconfig')['sumneko_lua'].setup {
--    capabilities = capabilities
--}
local nvim_lsp = require'lspconfig'

nvim_lsp.rust_analyzer.setup({
    capabilities=capabilities,
    settings = {
        ["rust-analyzer"] = {
            imports = {
                granularity = {
                    group = "module",
                },
                prefix = "self",
            },
            cargo = {
                buildScripts = {
                    enable = true,
                },
            },
            procMacro = {
                enable = true
            },
        }
    }
})

nvim_lsp.pyright.setup({
    capabilities=capabilities
})

nvim_lsp.clangd.setup({
    cmd = {'clangd', '-header-insertion=never'},
    capabilities=capabilities
})

nvim_lsp.zls.setup({
    capabilities=capabilities
})

nvim_lsp.ts_ls.setup({
    capabilities=capabilities
})


vim.api.nvim_create_autocmd('LspAttach', {
    group = vim.api.nvim_create_augroup('UserLspConfig', {}),
    callback = function(ev)
        local opts = { buffer = ev.buf}
        vim.keymap.set('n', '<C-a>', vim.lsp.buf.code_action, opts)
        vim.keymap.set('n', 'gD', vim.lsp.buf.declaration, opts)
        vim.keymap.set('n', 'K', function() vim.lsp.buf.hover({border = 'single'}) end, opts)
        vim.keymap.set('n', 'gd', vim.lsp.buf.definition, opts)
        vim.keymap.set('n', 'gi', vim.lsp.buf.implementation, opts)
        vim.keymap.set('n', 'gr', vim.lsp.buf.references, opts)
        vim.keymap.set('n', 'gt', vim.lsp.buf.type_definition, opts)
        vim.keymap.set('n', 'gR', vim.lsp.buf.rename, opts)
        vim.keymap.set('n', 'J', vim.diagnostic.open_float, opts)
    end
})

vim.o.keywordprg = ':help'

vim.highlight.priorities.semantic_tokens = 95

EOF

