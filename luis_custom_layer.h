#ifndef LUIS_CUSTOM_LAYER_H /* date = July 2nd 2021 8:59 pm */
#define LUIS_CUSTOM_LAYER_H

#if !SHIP_MODE
	#define assert(x) ((x) ? true : (*((i32*)0) = 1)) 
#else
	#define assert(x)  
#endif

#define foreach_index_inc(varname, array_count) for(i32 varname = 0; varname < (array_count); varname += 1)
#define countof(array) _countof(array) //nicer than sizeof(array)/sizeof(array[0]) because it'll emit error with pointers
#define CLAMP_HI(var, max) (((var) > (max)) ? ((var) = (max)) : (var))
#define CLAMP_LO(var, min) (((var) < (min)) ? ((var) = (min)) : (var))
#define CLAMP(var, min, max) (assert(max > min), CLAMP_LO(var, min), CLAMP_HI(var, max))


CUSTOM_ID(attachment, view_custom_flags);
CUSTOM_ID(attachment, view_code_peek_state);
CUSTOM_ID(attachment, view_tab_group_index);
//CUSTOM_ID(attachment, view_current_tab); not good idea, you'd have to store per view, per BUFFER_TAB_GROUP_COUNT

CUSTOM_ID(colors, luiscolor_type);
CUSTOM_ID(colors, luiscolor_macro);
CUSTOM_ID(colors, luiscolor_function);
CUSTOM_ID(colors, luiscolor_variable_decl);
global b32 IN_MODAL_MODE;
global b32 SHOW_BRACE_LINE_ANNOTATIONS;
global Face_ID SMALL_CODE_FACE;
global Face_ID ITALICS_CODE_FACE;
global Face_ID BOLD_CODE_FACE;

struct Peek_Code_Index_State
{
   Code_Index_Note *first_note;
   i32 index;
};

enum Custom_View_Flags 
{
   VIEW_IS_PEEK_WINDOW        = (1 << 0),
   VIEW_NOTEPAD_MODE_MARK_SET = (1 << 1),
   VIEW_ADD_NEW_BUFFER_AS_NEW_TAB = (1 << 2), //otherwise we overwrite current_tab
};

//NOTE(luis) you can build panel layouts into this and just store a global current_workspace
//but I tend to just prefer to have one panel open, so I wouldn't get much out of it
struct Buffer_Tab_Group
{
   Buffer_ID tabs[8];
   i32 tab_count;
   i32 current_tab;
};

global i32 BUFFER_TAB_GROUP_COUNT;
global i32 COMPILE_BUFFER_TAB = -1;
global Buffer_Tab_Group BUFFER_TAB_GROUPS[12]; //NOTE must always be greater than 2

function b32
luis_view_has_flags(Application_Links *app, View_ID view, u32 flags)
{
   Managed_Scope scope = view_get_managed_scope(app, view);
   u32 *view_flags = scope_attachment(app, scope, view_custom_flags, u32);
   b32 result = view_flags && ((*view_flags & flags) == flags);
   return result;
}

function void
luis_view_set_flags(Application_Links *app, View_ID view, u32 flags)
{
   Managed_Scope scope = view_get_managed_scope(app, view);
   u32 *view_flags = scope_attachment(app, scope, view_custom_flags, u32);
   if(view_flags)
      *view_flags |= flags;
}

function void
luis_view_clear_flags(Application_Links *app, View_ID view, u32 flags)
{
   Managed_Scope scope = view_get_managed_scope(app, view);
   u32 *view_flags = scope_attachment(app, scope, view_custom_flags, u32);
   if(view_flags)
      *view_flags &= ~flags;
}

internal b32
find_tab_with_buffer_id(Buffer_Tab_Group *group, Buffer_ID id, i32 *out_found_index)
{
   foreach_index_inc(i, group->tab_count)
      if(group->tabs[i] == id)
   	{
      	*out_found_index = i;
      	return true;
   	}
   return false;
}



internal View_ID
luis_get_other_child_view(Application_Links *app, View_ID view)
{
   View_ID bro_view = 0;
   Panel_ID view_panel = view_get_panel(app, view);
   Panel_ID parent_panel = panel_get_parent(app, view_panel);
   if(panel_is_split(app, parent_panel))
   {
      Panel_ID bro_panel = ((panel_get_child(app, parent_panel, Side_Min) == view_panel) ? 
                            panel_get_child(app, parent_panel, Side_Max) :
                            panel_get_child(app, parent_panel, Side_Min));
      if(panel_is_leaf(app, bro_panel))
      {
         bro_view = panel_get_view(app, bro_panel, Access_Always);
      }
   }
   return bro_view;
}

//if the view itself is a peek window, we return it
//otherwise we check it's sibling and return it if it's a peek window
internal View_ID
luis_get_peek_window(Application_Links *app, View_ID view)
{
   View_ID peek_view = 0;
   if(luis_view_has_flags(app, view, VIEW_IS_PEEK_WINDOW))
      peek_view = view;
   else
   {
      View_ID bro_view = luis_get_other_child_view(app, view);
      if(bro_view && luis_view_has_flags(app, bro_view, VIEW_IS_PEEK_WINDOW))
         peek_view = bro_view;
   }
   return peek_view;
}

//get view of whatever triggered this peek view
internal View_ID
luis_get_peek_windows_parent_view(Application_Links *app, View_ID peek)
{
   View_ID view = 0;
   if(luis_view_has_flags(app, peek, VIEW_IS_PEEK_WINDOW))
      view = luis_get_other_child_view(app, peek);
   return view;
}

function Peek_Code_Index_State *
luis_get_code_peek_state(Application_Links *app, View_ID view, String_Const_u8 identifier)
{
   if(luis_view_has_flags(app, view, VIEW_IS_PEEK_WINDOW))
      view = luis_get_peek_windows_parent_view(app, view);
   
   Managed_Scope scope = view_get_managed_scope(app, view);
   Peek_Code_Index_State *state = scope_attachment(app, scope, view_code_peek_state, Peek_Code_Index_State);
   if(state)
   {
      //NOTE code_index_note_from_string already checks to find first note that matches with string identifier
      Code_Index_Note *first_note = code_index_note_from_string(identifier);
      if(first_note)
      {
         if(state->first_note != first_note) //reset state if we were peeking something else
         {
            *state = {};
            state->first_note = first_note;
            //NOTE this is hacky but basically we want to jump to index 0 on first call to peek_prev or peek_next
            //but those always find a new code index by doing find(state, index + 1) or find(state, index - 1);
            //the convention is if index is negative just go to 0, so it works....
            state->index = -32; 
         }
      }
      else state = 0;
   }
   return state;
}





internal View_ID
luis_get_or_split_peek_window(Application_Links *app, View_ID view, View_Split_Position split_kind)
{
   View_ID peek = luis_get_peek_window(app, view);
   if(!peek)
   {
      peek = open_view(app, view, split_kind);
      if(peek)
      {
         luis_view_set_flags(app, peek, VIEW_IS_PEEK_WINDOW);
         Rect_f32 view_rect = view_get_screen_rect(app, view);
         view_set_split_pixel_size(app, peek, (i32)((view_rect.y1 - view_rect.y0)*0.25f));    
      }
      
   }
   return peek;
}

//also taken from RION
function b32
RION_is_file_readable(String_Const_u8 path)
{
   b32 result = 0;
   FILE *file = fopen((char *)path.str, "r");
   if(file)
   {
      result = 1;
      fclose(file);
   }
   return result;
}

//stole this from RION, thanks!
internal i32
RION_load_face_id(Application_Links *app, String_Const_u8 font_filename, i32 pt_size_delta_from_normal)
{
   Scratch_Block scratch(app);
   String_Const_u8 bin_path = system_get_path(scratch, SystemPath_Binary);
   
   // NOTE(rjf): Fallback font.
   Face_ID result = get_face_id(app, 0); 
   
   Face_Description normal_code_desc = get_face_description(app, get_face_id(app, 0));
   //i32 pt_size = normal_code_desc.parameters.pt_size;
   //i32 pt_size = (i32)def_get_config_u64(app, vars_save_string_lit("default_font_size"));
   i32 pt_size = (i32)def_get_config_u64(app, vars_save_string_lit("default_font_size"));
   pt_size += pt_size_delta_from_normal;
   
   if(pt_size > 0)
   {
      Face_Description desc = {};      
      desc.font.file_name =  push_u8_stringf(scratch, "%.*sfonts/%.*s", string_expand(bin_path), string_expand(font_filename));
      
      desc.parameters.pt_size = (u32)pt_size;
      desc.parameters.bold = 0;
      desc.parameters.italic = 0;
      desc.parameters.hinting = 0;
      
      if(RION_is_file_readable(desc.font.file_name)) 
         result = try_create_new_face(app, &desc);
   }
   
   
   return result;
}



function void 
luis_essential_mapping(Mapping *mapping, i64 global_id, i64 file_id, i64 code_id, i64 modal_id){
   MappingScope();
   SelectMapping(mapping);
   
   SelectMap(global_id);
   BindCore(luis_startup, CoreCode_Startup);
   BindCore(default_try_exit, CoreCode_TryExit);
   BindCore(clipboard_record_clip, CoreCode_NewClipboardContents);
   BindMouseWheel(mouse_wheel_scroll);
   BindMouseWheel(mouse_wheel_change_face_size, KeyCode_Control);
   //BindMouseRelease(click_set_cursor, MouseCode_Left);
   
   SelectMap(file_id);
   ParentMap(global_id);
   BindTextInput(write_text_input);
   BindMouse(click_set_cursor_and_mark, MouseCode_Left);
   BindMouseRelease(click_set_cursor, MouseCode_Left);
   BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
   BindMouseMove(click_set_cursor_if_lbutton);
   //BindMouseMove(my_mouse_drag);
   //BindMouse(mouse_select_token, MouseCode_Left, KeyCode_Control);
   //BindMouse(mouse_select_line,  MouseCode_Left, KeyCode_Alt);
   
   
   
   SelectMap(code_id);
   ParentMap(file_id);
   BindTextInput(write_text_input);
   //BindTextInput(write_text_and_auto_indent);
   
   SelectMap(modal_id);
   ParentMap(global_id);
   BindMouse(click_set_cursor_and_mark, MouseCode_Left);
   //BindMouse(mouse_select_token, MouseCode_Left, KeyCode_Control);
   //BindMouse(mouse_select_line,  MouseCode_Left, KeyCode_Alt);
   BindMouseRelease(click_set_cursor, MouseCode_Left);
   BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
   BindMouseMove(click_set_cursor_if_lbutton); 
}

internal Range_i64
get_visual_line_start_end_pos(Application_Links *app, View_ID view, i64 linenum)
{
   Range_i64 result = {};
   Scratch_Block scratch(app);
   Buffer_ID buffer_id = view_get_buffer(app, view, Access_Always);
   result.min = get_line_start_pos(app, buffer_id, linenum);
   result.max   = get_line_end_pos(app, buffer_id, linenum);
   String_Const_u8 string = push_buffer_line(app, scratch, buffer_id, linenum);
   result.min += string_find_first_non_whitespace(string);
   result.max -= string.size - (string_find_last_non_whitespace(string) + 1);
   return result;
}



internal void
center_view(Application_Links *app, View_ID view, float shift_y)
{
   //View_ID view = get_active_view(app, Access_ReadVisible);
   Rect_f32 region = view_get_buffer_region(app, view);
   i64 pos = view_get_cursor_pos(app, view);
   Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
   f32 view_height = rect_height(region);
   Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
   scroll.target.line_number = cursor.line;
   scroll.target.pixel_shift.y = -view_height*shift_y;
   view_set_buffer_scroll(app, view, scroll, SetBufferScroll_SnapCursorIntoView);
   no_mark_snap_to_cursor(app, view);
}

internal void
peek_next_code_index(Application_Links *app, View_ID view, Peek_Code_Index_State *state, i32 index)
{
   if(!state || !state->first_note) return;
   if(index < 0)	index = 0;
   
   Code_Index_Note *note = 0;
   i32 current_index = 0;
   for(Code_Index_Note *n = state->first_note; n; n = n->next_in_hash)
   {
      if(string_match(n->text, state->first_note->text))
      {
         if(current_index == index)
         {
            note = n;  
            break;
         }
         current_index += 1;
      }
   }
   
   if(note) 
   {
      View_ID peek = luis_get_or_split_peek_window(app, view, ViewSplit_Bottom);
      if(peek)
      {
         view_set_active(app, peek);
         view_set_buffer(app, peek, note->file->buffer, 0);
         view_set_cursor_and_preferred_x(app, peek, seek_pos(note->pos.first));
         luis_center_view_top(app);
         state->index = index;   
      }
   }
}

internal b32
find_next_scope_absolute(Application_Links *app, Buffer_ID buffer, i64 pos, Range_i64 *out_range)
{
   b32 found = false;
   Find_Nest_Flag flags = FindNest_Scope;
   Range_i64 range = {};
   if (find_nest_side(app, buffer, pos + 1, flags, Scan_Forward, NestDelim_Open,
                      &range) &&
       find_nest_side(app, buffer, range.end,
                      flags|FindNest_Balanced|FindNest_EndOfToken, Scan_Forward,
                      NestDelim_Close, &range.end))
   {
      
      found = true;
      *out_range = range;
   }
   return found;
}

internal b32
find_maximal_scope(Application_Links *app, Buffer_ID buffer, i64 pos, Range_i64 *out_range)
{
   b32 found = false;
   Range_i64 range = {};
   if (find_surrounding_nest(app, buffer, pos, FindNest_Scope, &range)){
      for (;;){
         pos = range.min;
         if (!find_surrounding_nest(app, buffer, pos, FindNest_Scope, &range)){
            break;
         }
      }
      found = true;
      *out_range = range;
   }
   return found;
}

internal b32 
is_paren_range(Application_Links *app, Buffer_ID buffer, Range_i64 range)
{	return buffer_get_char(app, buffer, range.min) == '(' && buffer_get_char(app, buffer, range.max-1) == ')';	}

internal b32
find_maximal_parens(Application_Links *app, Buffer_ID buffer, i64 pos, Range_i64 *out_range)
{
   b32 found = false;
   Range_i64 range = {};
   if(find_surrounding_nest(app, buffer, pos, FindNest_Paren, &range))
   {
      Range_i64 last_paren_range_found = {};
      if(is_paren_range(app, buffer, range)) last_paren_range_found = range;
         
      for (;;){
         pos = range.min;
         if(find_surrounding_nest(app, buffer, pos, FindNest_Paren, &range))
         {
            if(is_paren_range(app, buffer, range)) last_paren_range_found = range;
         }
         else break;
      }
      
      if(last_paren_range_found.max > last_paren_range_found.min)
      {
         found = true;
         *out_range = last_paren_range_found;
      }
   }
   return found;
}

internal b32
find_next_parens_absolute(Application_Links *app, Buffer_ID buffer, i64 pos, Range_i64 *out_range)
{
   b32 found = false;
   Find_Nest_Flag flags = FindNest_Paren;
   Range_i64 range = {};
   while(find_nest_side(app, buffer, pos + 1, flags, Scan_Forward, NestDelim_Open,
                        &range) &&
         find_nest_side(app, buffer, range.end,
                        flags|FindNest_Balanced|FindNest_EndOfToken, Scan_Forward,
                        NestDelim_Close, &range.end))
   {
      if(is_paren_range(app, buffer, range))
      {
         found = true;
         *out_range = range;
         break;
      }
      else pos = range.min;
   }
   return found;
}

internal void
add_fancy_strings_for_tab(Application_Links *app, Fancy_Line *list, Arena *scratch, Buffer_ID tab, b32 is_current_tab)
{
   FColor base_color = fcolor_id(defcolor_base);
   FColor pop2_color = fcolor_id(luiscolor_function);
   Assert(buffer_exists(app, tab));
   
   String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, tab);
   FColor color = base_color;
   if(is_current_tab) //highlight which tab we're on
   { 
      color = pop2_color;
      
   }
   push_fancy_string(scratch, list, color, unique_name);
   if(is_current_tab)
   {
      Dirty_State dirty = buffer_get_dirty_state(app, tab);
      u8 space[3];
      String_u8 str = Su8(space, 0, 3);
      if (HasFlag(dirty, DirtyState_UnsavedChanges)) string_append(&str, string_u8_litexpr("*"));
      if (HasFlag(dirty, DirtyState_UnloadedChanges)) string_append(&str, string_u8_litexpr("!"));
      push_fancy_string(scratch, list, pop2_color, str.string);
   }
   push_fancy_string(scratch, list, base_color,  SCu8(" "));
}

#define LOCAL_STRING_BUILDER(_name, _buffer_size) u8 buffer_for_ ## _name[_buffer_size]; String_Builder _name = make_string_builder(buffer_for_ ## _name, (_buffer_size))
#define PTR_IN_BASE_SIZE(pointer, base, size)   ( (u8 *)(pointer) >= (u8 *)(base) && (u8 *)(pointer) < ((u8 *)(base) + (size)))
#define PTR_IN_BASE_COUNT(pointer, base, count) ( (pointer) >= (base) && (pointer) <= ((base) + (count) - 1) )

struct String_Builder 
{
   u8 *buffer;
   u32 size;
   u32 current_string_offset;
   u32 current_string_length;
};

internal String_Builder
make_string_builder(u8 *buffer, i32 buffer_size)
{
   assert(buffer && buffer_size > 1);
   String_Builder builder = {};
   builder.buffer = buffer;
   builder.size = buffer_size;
   return builder;
}

internal void
append(String_Builder *builder, u8 *str, u64 length)
{
   u8 *at = builder->buffer + builder->current_string_offset + builder->current_string_length;
   assert(PTR_IN_BASE_COUNT(at, builder->buffer, builder->size));
   
   i32 amt_added = 0;
   i32 amt_left  = (i32)((builder->buffer + builder->size) - at - 1); //-1 for null terminator
   //assert(amt_left > 0);
   
   while(amt_added < amt_left && amt_added < length)
   {
      at[amt_added] = str[amt_added];
      assert(at[amt_added]); //otherwise length is incorrect
      amt_added += 1;
   }
   builder->current_string_length += amt_added;
}

internal void
append(String_Builder *builder, char *str) 
{   append(builder, (u8 *)str, cstring_length(str));   }

internal void
append(String_Builder *builder, String_Const_u8 string) 
{   append(builder, string.str, string.size);   }

#if 0
internal void
append_valist(String_Builder *builder, u8 *fmt, va_list list)
{
   u8 *at = builder->buffer + builder->current_string_offset + builder->current_string_length;
   assert(PTR_IN_BASE_COUNT(at, builder->buffer, builder->size));
   
   i32 amt_left  = (i32)((builder->buffer + builder->size) - at); //no -1, because  snprintf null terminates always 
   //assert(amt_left > 0);
   if(amt_left > 0)
   {
      i32 amt_added = vsnprintf((char *)at, (size_t)amt_left, (char *)fmt, list); 
      builder->current_string_length += amt_added;   
   }
}

internal void
appendf(String_Builder *builder, u8 *fmt, ...)
{   
   va_list list;
   va_start(list, fmt);
   append_valist(builder, fmt, list);
   va_end(list);
}

//this is shorthand for appendf, mkstr(), we have this we can inline it as function argument...
internal String_Const_u8
mkstrf(String_Builder *builder, u8 *fmt, ...)
{
   va_list list;
   va_start(list, fmt);
   append_valist(builder, fmt, list);
   va_end(list);
   return mkstr(builder);
}
#endif

internal void
remove_trailing_whitespace(String_Builder *builder)
{
   //foreach_index_dec(i, to_write.length) 
   for(u32 i = builder->current_string_offset + builder->current_string_length - 1;
       i >= builder->current_string_offset;
       i -= 1)
   {
      if(builder->buffer[i] == ' ' || builder->buffer[i] == '\t' ||
         builder->buffer[i] == '\n' || builder->buffer[i] == '\r')
      {
         builder->buffer[i] = 0;
         assert(builder->current_string_length > 0);
         builder->current_string_length -= 1;
      }
      else break;
   }
}

//NOTE final call to mkstr null terminates the current string and returns it
internal String_Const_u8
mkstr(String_Builder *builder)
{
   String_Const_u8 result = {};
   if(builder->current_string_length)
   {
      //ensure we have room to null terminate
      assert((builder->current_string_offset + builder->current_string_length) < builder->size);
      result = SCu8(builder->buffer + builder->current_string_offset, builder->current_string_length);
      //mkstr(builder->buffer + builder->current_string_offset,
      //builder->current_string_length,
      //builder->current_string_length + 1);
      result.str[result.size] = 0; 
      
      builder->current_string_offset += builder->current_string_length + 1;
      builder->current_string_length = 0;   
   }
   return result;
}




#endif //LUIS_CUSTOM_LAYER_H
