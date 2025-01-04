" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

syn case match

" Keywords
syn keyword     luvKeyword       alignof
syn keyword     luvKeyword       and
syn keyword     luvKeyword       as
syn keyword     luvKeyword       break
syn keyword     luvKeyword       byte
syn keyword     luvKeyword       cast
syn keyword     luvKeyword       check
syn keyword     luvKeyword       const
syn keyword     luvKeyword       continue
syn keyword     luvKeyword       def
syn keyword     luvKeyword       defer
syn keyword     luvKeyword       del
syn keyword     luvKeyword       elif
syn keyword     luvKeyword       else
syn keyword     luvKeyword       for
syn keyword     luvKeyword       foreign
syn keyword     luvKeyword       global
syn keyword     luvKeyword       goto
syn keyword     luvKeyword       handle
syn keyword     luvKeyword       if
syn keyword     luvKeyword       import
syn keyword     luvKeyword       in
syn keyword     luvKeyword       nonlocal
syn keyword     luvKeyword       not
syn keyword     luvKeyword       offsetof
syn keyword     luvKeyword       on
syn keyword     luvKeyword       or
syn keyword     luvKeyword       pass
syn keyword     luvKeyword       print
syn keyword     luvKeyword       relocate
syn keyword     luvKeyword       return
syn keyword     luvKeyword       sizeof
syn keyword     luvKeyword       struct
syn keyword     luvKeyword       typedef
syn keyword     luvKeyword       typeid
syn keyword     luvKeyword       typeof
syn keyword     luvKeyword       with
hi def link     luvKeyword       Keyword

syn keyword     luvConst true
syn keyword     luvConst false
syn keyword     luvConst null
hi def link     luvConst Constant

syn match luvDec /\-\?\<\([0-9][0-9_]*\)\(i8\|u8\|i16\|u16\|i32\|u32\|i64\|u64\)\?\>/
syn match luvHex /\-\?\<0x[0-9A-Fa-f][0-9A-Fa-f_]*\(i8\|u8\|i16\|u16\|i32\|u32\|i64\|u64\)\?\>/
syn match luvOct /\-\?\<0o[0-7][0-7_]*\(i8\|u8\|i16\|u16\|i32\|u32\|i64\|u64\)\?\>/
syn match luvBin /\-\?\<0b[01][01_]*\(i8\|u8\|i16\|u16\|i32\|u32\|i64\|u64\)\?\>/
hi def link     luvDec Number
hi def link     luvHex Number
hi def link     luvOct Number
hi def link     luvBin Number

syn match       luvConstVar '\<_*[A-Z][A-Z0-9_]*'
hi def link     luvConstVar Preproc

" Basic type declarations
syn keyword     luvType bool byte codepoint int float double
syn keyword     luvType u8 u16 u32 u64 i8 i16 i32 i64
syn keyword     luvType f16 f32 f64
syn keyword     luvType str
syn keyword     luvType const_char opaque const_opaque size_t
syn match       luvType '\<_*[A-Z]\>'
syn match       luvType '\<_*[A-Z][A-Z0-9]*[a-z][A-Za-z0-9_]*'
hi def link     luvType Type

" Builtin functions
syn keyword     luvBuiltins enumerate len range reversed self
syn keyword     luvBuiltins alloc
hi def link     luvBuiltins Function

" Decorator
syn region      luvDecorator start=+@+ end=+\w\++
hi def link     luvDecorator         Macro

" Strings
syn region luvString start=/"/ skip=/\\\\\|\\"/ end=/"/ contains=luvInterpolatedWrapper oneline
syn region luvInterpolatedWrapper start='\v(^|[^\\])\zs\\\(\s*' end='\v\s*\)' contained containedin=luvString contains=luvInterpolatedString,luvString oneline
" TODO: It'd be nice to have this turn back on syns above for CONST to get
" highlighting in e.g.: "stuff: \(CONST)"
syn match luvInterpolatedString "\v\w+(\(\))?" contained containedin=luvInterpolatedWrapper oneline
syn region luvRawString  start=+r'''+ skip=+\\'+ end=+'''+ keepend contains=@Spell


hi default link luvInterpolatedWrapper Delimiter
hi def link     luvString            String
hi def link     luvRawString         String

" Comments
syn keyword     luvTodo              contained TODO XXX BUG
syn cluster     luvCommentGroup      contains=luvTodo
syn region      luvComment           start="#" end="$" contains=@luvCommentGroup,@Spell

hi def link     luvComment           Comment
hi def link     luvTodo              Todo

syn sync minlines=500

let b:current_syntax = "luv"
