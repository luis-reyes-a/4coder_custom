

CUSTOM_COMMAND_SIG(luis_new_tab_group)
CUSTOM_DOC("make a new tab group")
{
   View_ID view = get_active_view(app, Access_Always);
   view_new_tab_group(app, view);
}

CUSTOM_COMMAND_SIG(luis_kill_tab_group)
CUSTOM_DOC("kill current tab group")
{	kill_tab_group(app, view_get_tab_group_index(app, get_active_view(app, Access_Always)));	}

CUSTOM_COMMAND_SIG(luis_tab_group_lister)
CUSTOM_DOC("switch to new tab group")
{
   View_ID view = get_active_view(app, Access_Always);
   Managed_Scope scope = view_get_managed_scope(app, view);
   i32 *tab_group_index = scope_attachment(app, scope, view_tab_group_index, i32);
   if(!tab_group_index)	return;
   
   Scratch_Block scratch(app);
   Lister_Block lister(app, scratch);
   lister_set_query(lister, SCu8("Switch to tab group..."));
   lister_set_default_handlers(lister);
 
   foreach_index_inc(group_index, countof(BUFFER_TAB_GROUPS))
   {
      Buffer_Tab_Group *group = BUFFER_TAB_GROUPS + group_index;
      if(group->tab_count > 0)
      {
         LOCAL_STRING_BUILDER(builder, 1024);
         foreach_index_inc(tab_index, group->tab_count)
         {
            String_Const_u8 name = push_buffer_unique_name(app, scratch, group->tabs[tab_index]);
            append(&builder, name);
            append(&builder, " ");
         }
         lister_add_item(lister, mkstr(&builder), SCu8(), (void*)(i64)group_index, 0);
      }
   }
   
   Lister_Result l_result = run_lister(app, lister);
   if(!l_result.canceled)
   {
      i32 new_tab_group_index = (i32)(i64)l_result.user_data;
      if(new_tab_group_index != *tab_group_index)
      {
         *tab_group_index = new_tab_group_index;
      }
   }
}

CUSTOM_COMMAND_SIG(luis_new_tab)
CUSTOM_DOC("make a new tab group")
{
   View_ID view = get_active_view(app, Access_Always);
   if(luis_view_has_flags(app, view, VIEW_IS_PEEK_WINDOW))
   {
      Buffer_ID buffer_id = view_get_buffer(app, view, Access_Always);
      View_ID parent = luis_get_peek_windows_parent_view(app, view);
      if(parent)
      {
         kill_tab_group(app, view_get_tab_group_index(app, view));
         view_close(app, view);
         
         luis_view_set_flags(app, parent, VIEW_ADD_NEW_BUFFER_AS_NEW_TAB);
         view_set_buffer(app, parent, buffer_id, 0);
      }
      //else //something weird happend
   }
   else
   {
      luis_view_set_flags(app, view, VIEW_ADD_NEW_BUFFER_AS_NEW_TAB);
      interactive_open_or_new(app);   
   }
}

internal void
luis_offset_tab(Application_Links *app, i32 offset)
{
   View_ID view = get_active_view(app, Access_Always);
   Managed_Scope scope = view_get_managed_scope(app, view);
   i32 *tab_group_index = scope_attachment(app, scope, view_tab_group_index, i32);
   if(tab_group_index)
   {
      Buffer_Tab_Group *group = BUFFER_TAB_GROUPS + *tab_group_index;
      group->current_tab += offset;
      if(group->current_tab >= group->tab_count)
         group->current_tab = 0;
      else if(group->current_tab < 0)
         group->current_tab = group->tab_count - 1;
   }
}

CUSTOM_COMMAND_SIG(luis_tab_prev)
CUSTOM_DOC("move prev tab")
{	luis_offset_tab(app, -1);	}

CUSTOM_COMMAND_SIG(luis_tab_next)
CUSTOM_DOC("move next tab")
{	luis_offset_tab(app, 1);	}
   

internal void 
update_buffer_bindings_for_modal_toggling(Application_Links *app, Buffer_ID buffer_id)
{
	//change buffer command map id
   Scratch_Block scratch(app);
	b32 treat_as_code = false;
   String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer_id);
	String_Const_u8 ext = string_file_extension(file_name);
	if (string_match(ext, string_u8_litexpr("cpp")) ||
       string_match(ext, string_u8_litexpr("h")) || 
       string_match(ext, string_u8_litexpr("c")) ||    
       string_match(ext, string_u8_litexpr("hpp")) ||       
       string_match(ext, string_u8_litexpr("cc")))  
   {
      treat_as_code = true;
      
   }
	
	Command_Map_ID map_id;
	if(IN_MODAL_MODE) map_id = vars_save_string_lit("keys_modal");
	else              map_id = treat_as_code ? vars_save_string_lit("keys_code") :vars_save_string_lit("keys_file");
	
	Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
	Command_Map_ID *map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID); 
	*map_id_ptr = map_id;
}

CUSTOM_COMMAND_SIG(luis_toggle_modal_mode)
CUSTOM_DOC("Toggles modal mode")
{	
	IN_MODAL_MODE = !IN_MODAL_MODE;
   
	View_ID view        = get_active_view(app, Access_Always);
	Buffer_ID buffer_id = view_get_buffer(app, view, Access_Always);
	
   update_buffer_bindings_for_modal_toggling(app, buffer_id);
}

CUSTOM_COMMAND_SIG(luis_escape)
CUSTOM_DOC("escape key")
{
   //does nothing, hanlded by view_input_handler
}

internal void
luis_offset_code_index(Application_Links *app, i32 offset)
{
   Scratch_Block scratch(app);
   String_Const_u8 identifier = push_token_or_word_under_active_cursor(app, scratch);
   View_ID view = get_active_view(app, Access_Always);
   Peek_Code_Index_State *state = luis_get_code_peek_state(app, view, identifier);
   if(!state)	return;
   
   i32 new_index = state->index + offset;
   if(new_index < 0)	new_index = 0;
   
   if(state->index != new_index)
   {
      Code_Index_Note *note = 0;
      i32 current_index = 0;
      for(Code_Index_Note *n = state->first_note; n; n = n->next_in_hash)
      {
         if(string_match(n->text, identifier))
         {
            if(current_index == new_index)
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
            state->index = new_index;   
         }
      }
   }
}

CUSTOM_COMMAND_SIG(luis_code_index_prev)
CUSTOM_DOC("prev code index")
{
   luis_offset_code_index(app, -1);
}

CUSTOM_COMMAND_SIG(luis_code_index_next)
CUSTOM_DOC("prev code index")
{
   luis_offset_code_index(app, 1);
}

CUSTOM_COMMAND_SIG(luis_home)
CUSTOM_DOC("go start of visual line")
{
   View_ID view = get_active_view(app, Access_Always);
   i64 linenum = get_line_number_from_pos(app, view_get_buffer(app, view, Access_Always), view_get_cursor_pos(app, view));
   Range_i64 range = get_visual_line_start_end_pos(app, view, linenum);
   view_set_cursor_and_preferred_x(app, view, seek_pos(range.min));
}

CUSTOM_COMMAND_SIG(luis_end)
CUSTOM_DOC("go end of visual line")
{
   View_ID view = get_active_view(app, Access_Always);
   i64 linenum = get_line_number_from_pos(app, view_get_buffer(app, view, Access_Always), view_get_cursor_pos(app, view));
   Range_i64 range = get_visual_line_start_end_pos(app, view, linenum);
   view_set_cursor_and_preferred_x(app, view, seek_pos(range.max));
}

internal void
luis_set_mark(Application_Links *app, View_ID view, i64 pos)
{
   luis_view_set_flags(app, view, VIEW_NOTEPAD_MODE_MARK_SET);
   view_set_mark(app, view, seek_pos(pos));
}

CUSTOM_COMMAND_SIG(luis_set_mark)
CUSTOM_DOC("set mark")
{
   View_ID view = get_active_view(app, Access_Always);
   luis_set_mark(app, view, view_get_cursor_pos(app, view));
}

CUSTOM_COMMAND_SIG(luis_select_line)
CUSTOM_DOC("go end of visual line")
{
   View_ID view = get_active_view(app, Access_Always);
   i64 linenum = get_line_number_from_pos(app, view_get_buffer(app, view, Access_Always), view_get_cursor_pos(app, view));
   Range_i64 range = get_visual_line_start_end_pos(app, view, linenum);
   luis_set_mark(app, view, range.min);
   view_set_cursor_and_preferred_x(app, view, seek_pos(range.max));
}

CUSTOM_COMMAND_SIG(luis_build)
CUSTOM_DOC("build")
{
   if(is_valid_tab_group_index(BUFFER_TAB_GROUP_COMPILATION))
      kill_tab_group(app, BUFFER_TAB_GROUP_COMPILATION);
   if(is_valid_tab_group_index(BUFFER_TAB_GROUP_COMPILATION_SCRATCH))
      kill_tab_group(app, BUFFER_TAB_GROUP_COMPILATION_SCRATCH);
   
   View_ID view = get_active_view(app, Access_Always);
   Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
   
   View_ID build_view = luis_get_or_split_peek_window(app, view, ViewSplit_Bottom);
   if(build_view)
   {
      standard_search_and_build(app, build_view, buffer);
      set_fancy_compilation_buffer_font(app);
      
      block_zero_struct(&prev_location);
      lock_jump_buffer(app, string_u8_litexpr("*compilation*"));
      
      view_new_tab_group(app, view);
      BUFFER_TAB_GROUP_COMPILATION_SCRATCH = view_get_tab_group_index(app, view);
      BUFFER_TAB_GROUP_COMPILATION = view_get_tab_group_index(app, build_view);
   }
}

//this is very complicated but it's behaviour that makes sense to me
//first we try to close the "build panel"
//if no build panel, then we try to close the peek panel
//if no peek panel, we try to close a tab
//if there was only one tab to begin with, we just close that main panel
//NOTE(luis) calling close_panel() or close_build_panel() will most likely cause bugs
CUSTOM_COMMAND_SIG(luis_close_tab_or_panel)
CUSTOM_DOC("try to close something :)")
{
   b32 closed_build_view = false;
   if(is_valid_tab_group_index(BUFFER_TAB_GROUP_COMPILATION)) //*compile* tab group open
   {
      //find view with compilation tab group and close it
      for(View_ID v = get_view_next(app, 0, Access_Always); v; v = get_view_next(app, v, Access_Always))
      {
         if(luis_view_has_flags(app, v, VIEW_IS_PEEK_WINDOW) && view_get_tab_group_index(app, v) == BUFFER_TAB_GROUP_COMPILATION)
         {
            view_close(app, v);
            closed_build_view = true;
         }
      }
      kill_tab_group(app, BUFFER_TAB_GROUP_COMPILATION);
      BUFFER_TAB_GROUP_COMPILATION = -1;
   }
   if(is_valid_tab_group_index(BUFFER_TAB_GROUP_COMPILATION_SCRATCH)) //*compile* tab group open
   {
      kill_tab_group(app, BUFFER_TAB_GROUP_COMPILATION_SCRATCH);
      BUFFER_TAB_GROUP_COMPILATION_SCRATCH = -1;
   }
   
   if(!closed_build_view)
   {
      b32 close_panel_instead = false;
      View_ID view = get_active_view(app, Access_Always);
      View_ID peek = luis_get_peek_window(app, view);
      if(peek)
      {
         kill_tab_group(app, view_get_tab_group_index(app, peek));
         view_close(app, peek);
      }
      else //try to close a tab
      {
         Managed_Scope scope = view_get_managed_scope(app, view);
         i32 *tab_group_index = scope_attachment(app, scope, view_tab_group_index, i32);
         if(tab_group_index) 
         {
            Buffer_Tab_Group *group = BUFFER_TAB_GROUPS + *tab_group_index;
            if(group->tab_count > 1)
            {
               for(i32 i = group->current_tab; i < (group->tab_count - 1); i += 1)
                  group->tabs[i] = group->tabs[i+1];
               group->tab_count -= 1;
               
               if(group->current_tab >= group->tab_count)
                  group->current_tab = group->tab_count - 1;
            }
            else close_panel_instead = true;
         }
         else close_panel_instead = true;   
      }
      
      if(close_panel_instead)
         view_close(app, view);
   }  
}

CUSTOM_COMMAND_SIG(luis_left_word)
CUSTOM_DOC("move left")
{
   Scratch_Block scratch(app);
   current_view_scan_move(app, Scan_Backward, push_boundary_list(scratch, boundary_alpha_numeric_underscore));
}

CUSTOM_COMMAND_SIG(luis_right_word)
CUSTOM_DOC("move right")
{
   Scratch_Block scratch(app);
   current_view_scan_move(app, Scan_Forward, push_boundary_list(scratch, boundary_alpha_numeric_underscore));
}

CUSTOM_COMMAND_SIG(luis_center_view_top)
CUSTOM_DOC("Centers the view vertically on the line on which the cursor sits.")
{	center_view(app, get_active_view(app, Access_ReadVisible), 0.05f);	}


CUSTOM_COMMAND_SIG(luis_write_underscore)
CUSTOM_DOC("")
{	write_text(app, SCu8("_"));	}

CUSTOM_COMMAND_SIG(luis_write_pointer_arrow)
CUSTOM_DOC("")
{	write_text(app, SCu8("->"));	}

CUSTOM_COMMAND_SIG(luis_write_newline)
CUSTOM_DOC("")
{	write_text(app, SCu8("\n"));	}

CUSTOM_COMMAND_SIG(luis_write_tab)
CUSTOM_DOC("")
{	write_text(app, SCu8("\t"));	}

CUSTOM_COMMAND_SIG(luis_multiline_comment_toggle)
CUSTOM_DOC("Deletes all whitespace at cursor, going backwards")
{
   View_ID view = get_active_view(app, Access_ReadWriteVisible);
   Buffer_ID buffer_id = view_get_buffer(app, view, Access_ReadWriteVisible);
   i64 cursor_pos = view_get_cursor_pos(app, view);
   i64 mark_pos = view_get_mark_pos(app, view);
   b32 add_comments;
   {
      Range_i64 line_range = get_visual_line_start_end_pos(app, view, get_line_number_from_pos(app, buffer_id, cursor_pos));
      add_comments = !c_line_comment_starts_at_position(app, buffer_id, line_range.min);
   }
   ///   
   Range_i64 range = {};
   range.start = Min(cursor_pos, mark_pos);
   range.end   = Max(cursor_pos, mark_pos);
   Range_i64 lines = get_line_range_from_pos_range(app, buffer_id, range);
   if(!is_valid_line_range(app, buffer_id, lines))	return;
   
   History_Group new_history_group = history_group_begin(app, buffer_id);
   for(i64 line = lines.start; line <= lines.end; line += 1)
   {
      if(!line_is_blank(app, buffer_id, line))
      {
         //i64 pos = get_line_start_pos(app, buffer_id, line);
         //i64 pos = get_visual_line_start(app, view, buffer_id, line);
         i64 pos = get_visual_line_start_end_pos(app, view, line).min;
         
         u8  test[256];
         buffer_read_range(app, buffer_id, Ii64(pos, pos + 256), test);
         if(add_comments)
         {
            if(!c_line_comment_starts_at_position(app, buffer_id, pos))
            {
               buffer_replace_range(app, buffer_id, Ii64(pos), SCu8("//"));
            }
         }
         else
         {
            if(c_line_comment_starts_at_position(app, buffer_id, pos))
            {
               buffer_replace_range(app, buffer_id, Ii64(pos, pos + 2), string_u8_empty);
            }
         }
      }
   }
   history_group_end(new_history_group);
}

CUSTOM_COMMAND_SIG(luis_surround_in_parens)
CUSTOM_DOC("surrounds in ()")
{
	View_ID view = get_active_view(app, Access_Always);
   i64 cursor_pos = view_get_cursor_pos(app, view);
   i64 mark_pos = view_get_mark_pos(app, view);
   if(cursor_pos == mark_pos) return;

   Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
   
   History_Group group = history_group_begin(app, buffer);
   if(cursor_pos < mark_pos)
   {
      buffer_replace_range(app, buffer, Ii64(cursor_pos), SCu8("("));
      buffer_replace_range(app, buffer, Ii64(mark_pos + 1), SCu8(")"));
   }
   else
   {
      buffer_replace_range(app, buffer, Ii64(mark_pos), SCu8("("));
      buffer_replace_range(app, buffer, Ii64(cursor_pos + 1), SCu8(")"));
   }
   history_group_end(group);
}

internal b32
strmatch_so_far(String_Const_u8 a, String_Const_u8 b, i32 count)
{
   if(a.size >= count && b.size >= count)
   {
      for(i32 i = 0; i < count; i += 1)
      {
         if(a.str[i] != b.str[i])	return false;
      }
      return true;
   }
   else return false;  
}

CUSTOM_COMMAND_SIG(luis_scope_braces)
CUSTOM_DOC("writes {}")
{
   #if 1
   //write_text(app, SCu8("\n{\n\n}"));
   View_ID view = get_active_view(app, Access_ReadWriteVisible);
   Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
   Scratch_Block scratch(app);
   i64 pos = view_get_cursor_pos(app, view);
   i64 linenum = get_line_number_from_pos(app, buffer, pos);
   String_Const_u8 line = push_buffer_line(app, scratch, buffer, linenum);
   line = string_skip_whitespace(line);
   
   
   History_Group hgroup = history_group_begin(app, buffer);
   //NOTE I check for space after the keyword to ensure it's not a substring of a bigger word
   //this will miss if there's a newline right after it. The more correct way of doing this is with tokens
   String_Const_u8 string = {}; 
   if(strmatch_so_far(SCu8("struct "), line, 7) ||
      strmatch_so_far(SCu8("enum "),   line, 5) ||
      strmatch_so_far(SCu8("union "),  line, 6))
   {
      string.str = (u8 *)"\n{\n\n};";
      string.size = sizeof("\n{\n\n};") - 1;
   }
   else if(line.size == 0)
   {
      string.str = (u8 *)"{\n\n}";
      string.size = sizeof("{\n\n}") - 1;
      move_up(app);
   }
   else
   {
      string.str = (u8 *)"\n{\n\n}";
      string.size = sizeof("\n{\n\n}") - 1;
   }
   
   buffer_replace_range(app, buffer, Ii64(pos), string);
   auto_indent_buffer(app, buffer, Ii64_size(pos, string.size));
   move_vertical_lines(app, 2);
   auto_indent_line_at_cursor(app);
   history_group_end(hgroup);
   #endif
}

internal void
luis_isearch(Application_Links *app, Scan_Direction start_scan, i64 first_pos, String_Const_u8 query_init)
{
   View_ID view = get_active_view(app, Access_ReadVisible);
   Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
   i64 buffer_size = buffer_get_size(app, buffer);
   if(buffer_size == 0)	return;
   
   
   
   Query_Bar_Group group(app);
   Query_Bar bar = {};
   if(!start_query_bar(app, &bar, 0))	return;
   
   Vec2_f32 old_margin = {};
   Vec2_f32 old_push_in = {};
   view_get_camera_bounds(app, view, &old_margin, &old_push_in);
   
   Vec2_f32 margin = old_margin;
   margin.y = clamp_bot(200.f, margin.y);
   view_set_camera_bounds(app, view, margin, old_push_in);
   
   Scan_Direction scan = start_scan;
   i64 pos = first_pos;
   
   u8 bar_string_space[256];
   bar.string = SCu8(bar_string_space, query_init.size);
   block_copy(bar.string.str, query_init.str, query_init.size);
   u64 match_size = bar.string.size;
   
   #define BAR_APPEND_STRING(string__) \
   do \
   { \
      String_Const_u8 string = (string__); \
      String_u8 bar_string = Su8(bar.string, sizeof(bar_string_space)); \
      string_append(&bar_string, string); \
      bar.string = bar_string.string; \
      string_change = true; \
   } while(0)
   
   b32 move_to_new_pos_and_save_search_string = false;
   User_Input in = {};
   for (;;)
   {
      bar.prompt = (scan == Scan_Forward) ? string_u8_litexpr("I-Search: ") : string_u8_litexpr("Reverse-I-Search: ");
      isearch__update_highlight(app, view, Ii64_size(pos, match_size));
      
      in = get_next_input(app, EventPropertyGroup_Any, EventProperty_Escape);
      if (in.abort)	break;
      
      b32 string_change = false;
      b32 do_scan_action = false;
      b32 do_scroll_wheel = false;
      Scan_Direction change_scan = scan;
      if(in.event.kind == InputEventKind_KeyStroke)
      {
         Key_Code code = in.event.key.code;
         b32 ctrl_down = has_modifier(&in.event.key.modifiers, KeyCode_Control);
         
         if(code == KeyCode_Return || code == KeyCode_Tab)
         {
            if(ctrl_down) //append previous search string
            {
               bar.string.size = cstring_length(previous_isearch_query);
               block_copy(bar.string.str, previous_isearch_query, bar.string.size);
            }
            else
            {
               u64 size = bar.string.size;
               size = clamp_top(size, sizeof(previous_isearch_query) - 1);
               block_copy(previous_isearch_query, bar.string.str, size);
               previous_isearch_query[size] = 0;
               move_to_new_pos_and_save_search_string = true;
               break;
            }
         }
         else if(code == KeyCode_Backspace)
         {
            if(ctrl_down)
            {
               if (bar.string.size > 0){
                  string_change = true;
                  bar.string.size = 0;
               }
            }
            else
            {
               u64 old_bar_string_size = bar.string.size;
               bar.string = backspace_utf8(bar.string);
               string_change = (bar.string.size < old_bar_string_size);
            }
         }
         else
         {
            View_Context ctx = view_current_context(app, view);
            Mapping *mapping = ctx.mapping;
            Command_Map *map = mapping_get_map(mapping, ctx.map_id);
            Command_Binding binding = map_get_binding_recursive(mapping, map, &in.event);
            if (binding.custom != 0)
            {
               if (binding.custom == luis_fsearch)
               {
                  change_scan = Scan_Forward;
                  do_scan_action = true;
               }
               else if (binding.custom == luis_rsearch)
               {
                  change_scan = Scan_Backward;
                  do_scan_action = true;
               }
               else if (binding.custom == luis_write_underscore)
                  BAR_APPEND_STRING(SCu8("_"));
               else if (binding.custom == luis_write_pointer_arrow)
                  BAR_APPEND_STRING(SCu8("->"));
               else
               {
                  Command_Metadata *metadata = get_command_metadata(binding.custom);
                  if (metadata != 0){
                     if (metadata->is_ui){
                        view_enqueue_command_function(app, view, binding.custom);
                        break;
                     }
                  }
                  binding.custom(app);
               }
            }
            else	leave_current_input_unhandled(app);
         }
            
      }
      else if(in.event.kind == InputEventKind_TextInsert)
         BAR_APPEND_STRING(to_writable(&in));
      
      if (string_change){
         switch (scan){
            case Scan_Forward:
            {
               i64 new_pos = 0;
               seek_string_insensitive_forward(app, buffer, pos - 1, 0, bar.string, &new_pos);
               if (new_pos < buffer_size){
                  pos = new_pos;
                  match_size = bar.string.size;
               }
            }break;
            
            case Scan_Backward:
            {
               i64 new_pos = 0;
               seek_string_insensitive_backward(app, buffer, pos + 1, 0, bar.string, &new_pos);
               if (new_pos >= 0){
                  pos = new_pos;
                  match_size = bar.string.size;
               }
            }break;
         }
      }
      else if (do_scan_action){
         scan = change_scan;
         switch (scan){
            case Scan_Forward:
            {
               i64 new_pos = 0;
               seek_string_insensitive_forward(app, buffer, pos, 0, bar.string, &new_pos);
               if (new_pos < buffer_size){
                  pos = new_pos;
                  match_size = bar.string.size;
               }
            }break;
            
            case Scan_Backward:
            {
               i64 new_pos = 0;
               seek_string_insensitive_backward(app, buffer, pos, 0, bar.string, &new_pos);
               if (new_pos >= 0){
                  pos = new_pos;
                  match_size = bar.string.size;
               }
            }break;
         }
      }
      else if (do_scroll_wheel){
         mouse_wheel_scroll(app);
      }
   }
   
   view_disable_highlight_range(app, view);
   
   if (move_to_new_pos_and_save_search_string)
   {
      u64 size = bar.string.size;
      size = clamp_top(size, sizeof(previous_isearch_query) - 1);
      block_copy(previous_isearch_query, bar.string.str, size);
      previous_isearch_query[size] = 0;
      view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
   }
   else view_set_cursor_and_preferred_x(app, view, seek_pos(first_pos));
   
   view_set_camera_bounds(app, view, old_margin, old_push_in);
}

CUSTOM_COMMAND_SIG(luis_fsearch)
CUSTOM_DOC("search forwards")
{
   View_ID view = get_active_view(app, Access_Always);
   luis_isearch(app, Scan_Forward, view_get_cursor_pos(app, view), SCu8());
}

CUSTOM_COMMAND_SIG(luis_rsearch)
CUSTOM_DOC("search backwards")
{
   View_ID view = get_active_view(app, Access_Always);
   luis_isearch(app, Scan_Backward, view_get_cursor_pos(app, view), SCu8());
}

CUSTOM_COMMAND_SIG(luis_list_exact_matches_of_identifier)
CUSTOM_DOC("find matches of identifier under cursor")
{
   Scratch_Block scratch(app);
   String_Const_u8 needle = push_token_or_word_under_active_cursor(app, scratch);
   if(needle.size > 0)
   {
      View_ID peek = luis_get_or_split_peek_window(app, get_active_view(app, Access_Always), ViewSplit_Bottom);
      if(peek)
      {
         String_Const_u8_Array search_array = {&needle, 1};
         String_Match_Flag must_have_flags = 0;
         AddFlag(must_have_flags, StringMatch_CaseSensitive);
         String_Match_Flag must_not_have_flags = 0;
         //AddFlag(must_not_have_flags, StringMatch_LeftSideSloppy);
         //AddFlag(must_not_have_flags, StringMatch_RightSideSloppy);
         Buffer_ID search_buffer = create_or_switch_to_buffer_and_clear_by_name(app, search_name, peek);
         print_all_matches_all_buffers(app, search_array, must_have_flags, must_not_have_flags, search_buffer);
      }
   }
}

CUSTOM_COMMAND_SIG(luis_toggle_matching_cpp)
CUSTOM_DOC("If the current file is a *.cpp or *.h, attempts to open the corresponding *.h or *.cpp file in the other view.")
{
   View_ID view = get_active_view(app, Access_Always);
   Buffer_Tab_Group *group = view_get_tab_group(app, view);
   if(group)
   {
      Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
      Buffer_ID new_buffer = 0;
      if(get_cpp_matching_file(app, buffer, &new_buffer))
         group->tabs[group->current_tab] = new_buffer;
   }
}


function Lister_Result
luis_run_lister(Application_Links *app, Lister *lister, i32 best_starting_index = 0)
{
   lister->filter_restore_point = begin_temp(lister->arena);
   lister_update_filtered_list(app, lister);
   
   View_ID view = get_this_ctx_view(app, Access_Always);
   View_Context ctx = view_current_context(app, view);
   ctx.render_caller = lister_render;
   ctx.hides_buffer = true;
   View_Context_Block ctx_block(app, view, &ctx);
   
   if (lister->handlers.navigate && best_starting_index > 0)
      lister->handlers.navigate(app, view, lister, best_starting_index);
   
   for (;;){
      User_Input in = get_next_input(app, EventPropertyGroup_Any, EventProperty_Escape);
      if (in.abort){
         block_zero_struct(&lister->out);
         lister->out.canceled = true;
         break;
      }
      
      Lister_Activation_Code result = ListerActivation_Continue;
      b32 handled = true;
      switch (in.event.kind){
         case InputEventKind_TextInsert:
         {
            if (lister->handlers.write_character != 0){
               result = lister->handlers.write_character(app);
            }
         }break;
         
         case InputEventKind_KeyStroke:
         {
            switch (in.event.key.code){
               case KeyCode_Return:
               case KeyCode_Tab:
               {
                  void *user_data = 0;
                  if (0 <= lister->raw_item_index &&
                      lister->raw_item_index < lister->options.count){
                     user_data = lister_get_user_data(lister, lister->raw_item_index);
                  }
                  lister_activate(app, lister, user_data, false);
                  result = ListerActivation_Finished;
               }break;
               
               case KeyCode_Backspace:
               {
                  if (lister->handlers.backspace != 0){
                     lister->handlers.backspace(app);
                  }
                  else if (lister->handlers.key_stroke != 0){
                     result = lister->handlers.key_stroke(app);
                  }
                  else{
                     handled = false;
                  }
               }break;
               
               case KeyCode_Up:
               {
                  if (lister->handlers.navigate != 0){
                     lister->handlers.navigate(app, view, lister, -1);
                  }
                  else if (lister->handlers.key_stroke != 0){
                     result = lister->handlers.key_stroke(app);
                  }
                  else{
                     handled = false;
                  }
               }break;
               
               case KeyCode_Down:
               {
                  if (lister->handlers.navigate != 0){
                     lister->handlers.navigate(app, view, lister, 1);
                  }
                  else if (lister->handlers.key_stroke != 0){
                     result = lister->handlers.key_stroke(app);
                  }
                  else{
                     handled = false;
                  }
               }break;
               
               case KeyCode_PageUp:
               {
                  if (lister->handlers.navigate != 0){
                     lister->handlers.navigate(app, view, lister,
                                               -lister->visible_count);
                  }
                  else if (lister->handlers.key_stroke != 0){
                     result = lister->handlers.key_stroke(app);
                  }
                  else{
                     handled = false;
                  }
               }break;
               
               case KeyCode_PageDown:
               {
                  if (lister->handlers.navigate != 0){
                     lister->handlers.navigate(app, view, lister,
                                               lister->visible_count);
                  }
                  else if (lister->handlers.key_stroke != 0){
                     result = lister->handlers.key_stroke(app);
                  }
                  else{
                     handled = false;
                  }
               }break;
               
               default:
               {
                  if (lister->handlers.key_stroke != 0){
                     result = lister->handlers.key_stroke(app);
                  }
                  else{
                     handled = false;
                  }
               }break;
            }
         }break;
         
         case InputEventKind_MouseButton:
         {
            switch (in.event.mouse.code){
               case MouseCode_Left:
               {
                  Vec2_f32 p = V2f32(in.event.mouse.p);
                  void *clicked = lister_user_data_at_p(app, view, lister, p);
                  lister->hot_user_data = clicked;
               }break;
               
               default:
               {
                  handled = false;
               }break;
            }
         }break;
         
         case InputEventKind_MouseButtonRelease:
         {
            switch (in.event.mouse.code){
               case MouseCode_Left:
               {
                  if (lister->hot_user_data != 0){
                     Vec2_f32 p = V2f32(in.event.mouse.p);
                     void *clicked = lister_user_data_at_p(app, view, lister, p);
                     if (lister->hot_user_data == clicked){
                        lister_activate(app, lister, clicked, true);
                        result = ListerActivation_Finished;
                     }
                  }
                  lister->hot_user_data = 0;
               }break;
               
               default:
               {
                  handled = false;
               }break;
            }
         }break;
         
         case InputEventKind_MouseWheel:
         {
            Mouse_State mouse = get_mouse_state(app);
            lister->scroll.target.y += mouse.wheel;
            lister_update_filtered_list(app, lister);
         }break;
         
         case InputEventKind_MouseMove:
         {
            lister_update_filtered_list(app, lister);
         }break;
         
         case InputEventKind_Core:
         {
            switch (in.event.core.code){
               case CoreCode_Animate:
               {
                  lister_update_filtered_list(app, lister);
               }break;
               
               default:
               {
                  handled = false;
               }break;
            }
         }break;
         
         default:
         {
            handled = false;
         }break;
      }
      
      if (result == ListerActivation_Finished){
         break;
      }
      
      if (!handled){
         Mapping *mapping = lister->mapping;
         Command_Map *map = lister->map;
         
         Fallback_Dispatch_Result disp_result =
            fallback_command_dispatch(app, mapping, map, &in);
         if (disp_result.code == FallbackDispatch_DelayedUICall){
            call_after_ctx_shutdown(app, view, disp_result.func);
            break;
         }
         if (disp_result.code == FallbackDispatch_Unhandled){
            leave_current_input_unhandled(app);
         }
         else{
            lister_call_refresh_handler(app, lister);
         }
      }
   }
   
   return(lister->out);
}

CUSTOM_COMMAND_SIG(luis_show_buffer_code_notes)
CUSTOM_DOC("show all the code notes found for this buffer")
{
   View_ID view = get_active_view(app, Access_Always);
   Buffer_ID buffer_id = view_get_buffer(app, view, Access_Always);
   i64 pos = view_get_cursor_pos(app, view);
   //i64 linenum = get_line_number_from_pos(app, buffer_id, pos);
   
   Scratch_Block scratch(app);
   Lister_Block lister(app, scratch);
   lister_set_query(lister, SCu8("Index: "));
   lister_set_default_handlers(lister);
   
   i32 best_starting_index = 0;
   i64 smallest_diff = max_i64;
   Code_Index_File *file = code_index_get_file(buffer_id);
   if(file)
   {
      for(i32 i = 0; i < file->note_array.count; i += 1)
      {
         Code_Index_Note *note = file->note_array.ptrs[i];
         i64 diff = abs(note->pos.first - pos); 
         if(diff < smallest_diff)
         {
            smallest_diff = diff;
            best_starting_index = lister.lister.current->options.count;
         }
         
         if(!note->parent) //only add top level notes
         {
            String_Const_u8 status = push_buffer_line(app, scratch, buffer_id, get_line_number_from_pos(app, buffer_id, note->pos.min));
            lister_add_item(lister, note->text, status, (void*)note, 0);   
         }
         
      }
      
      //lister.lister.current->item_index = best_starting_index; //NOTE set item_index, with clamping and whatever
      //lister.lister.current->set_vertical_focus_to_item = true;
      //lister_update_selection_values(lister.lister.current);
      
      Lister_Result l_result = luis_run_lister(app, lister, best_starting_index);
      if (!l_result.canceled)
      {
         Code_Index_Note *note = (Code_Index_Note *)l_result.user_data;
         view_set_cursor_and_preferred_x(app, view, seek_pos(note->pos.first));
         luis_center_view_top(app);
      }
   }
}

internal void
show_buffer_code_notes_all_buffers(Application_Links *app, b32 show_functions, b32 show_types, b32 show_macros)
{
   //View_ID view = get_active_view(app, Access_Always);
   //Buffer_ID buffer_id = view_get_buffer(app, view, Access_Always);
   
   Scratch_Block scratch(app);
   Lister_Block lister(app, scratch);
   lister_set_query(lister, SCu8("Index: "));
   lister_set_default_handlers(lister);
   
   for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
        buffer != 0;
        buffer = get_buffer_next(app, buffer, Access_Always))
   {
      Code_Index_File *file = code_index_get_file(buffer);
      if(!file) continue;
      
      for(i32 i = 0; i < file->note_array.count; i += 1)
      {
         Code_Index_Note *note = file->note_array.ptrs[i];
         if(!note->parent) //only add top level notes
         {
            if((note->note_kind == CodeIndexNote_Function && show_functions) ||
               (note->note_kind == CodeIndexNote_Type     && show_types) ||
               (note->note_kind == CodeIndexNote_Macro    && show_macros))
            {
               String_Const_u8 status = push_buffer_line(app, scratch, note->file->buffer, get_line_number_from_pos(app, note->file->buffer, note->pos.min));
               lister_add_item(lister, note->text, status, (void*)note, 0);   
            }
            
         }
      }
   }
   
   Lister_Result l_result = luis_run_lister(app, lister);
   if (!l_result.canceled)
   {
      Code_Index_Note *note = (Code_Index_Note *)l_result.user_data;
      View_ID view = get_active_view(app, Access_Always);
      view_set_buffer(app, view, note->file->buffer, 0);
      view_set_cursor_and_preferred_x(app, view, seek_pos(note->pos.first));
      luis_center_view_top(app);
   }
}

CUSTOM_COMMAND_SIG(luis_show_functions)
CUSTOM_DOC("show all functions")
{
   b32 show_functions = true;
   b32 show_types     = false;
   b32 show_macros    = false;
   show_buffer_code_notes_all_buffers(app, show_functions, show_types, show_macros);
}

CUSTOM_COMMAND_SIG(luis_show_types)
CUSTOM_DOC("show all types")
{
   b32 show_functions = false;
   b32 show_types     = true;
   b32 show_macros    = false;
   show_buffer_code_notes_all_buffers(app, show_functions, show_types, show_macros);
}

CUSTOM_COMMAND_SIG(luis_show_all)
CUSTOM_DOC("show all codes notes")
{
   show_buffer_code_notes_all_buffers(app, true, true, true);
}



function void
luis_select_scope(Application_Links *app, View_ID view, Range_i64 range){
   view_set_cursor_and_preferred_x(app, view, seek_pos(range.first));
   luis_set_mark(app, view, range.end);
   view_look_at_region(app, view, range.first, range.end);
   no_mark_snap_to_cursor(app, view);
}

CUSTOM_COMMAND_SIG(luis_select_surrounding_scope)
CUSTOM_DOC("select surrounding scope")
{
   View_ID view = get_active_view(app, Access_ReadVisible);
   Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
   i64 pos = view_get_cursor_pos(app, view);
   Range_i64 range = {};
   if (find_surrounding_nest(app, buffer, pos, FindNest_Scope, &range)){
      luis_select_scope(app, view, range);
   }
}

CUSTOM_COMMAND_SIG(luis_select_surrounding_scope_maximal)
CUSTOM_DOC("select surrounding scope")
{
   View_ID view = get_active_view(app, Access_ReadVisible);
   Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
   i64 pos = view_get_cursor_pos(app, view);
   Range_i64 range = {};
   if (find_surrounding_nest(app, buffer, pos, FindNest_Scope, &range)){
      for (;;){
         pos = range.min;
         if (!find_surrounding_nest(app, buffer, pos, FindNest_Scope, &range)){
            break;
         }
      }
      luis_select_scope(app, view, range);
   }
}

global i32 ON_MOUSE_CLICK_TAB_INDEX = -1;
CUSTOM_COMMAND_SIG(luis_mouse_click)
CUSTOM_DOC("Sets the cursor position and mark to the mouse position.")
{
   View_ID view = get_active_view(app, Access_ReadVisible);
   Mouse_State mouse = get_mouse_state(app);
   ON_MOUSE_CLICK_TAB_INDEX = -1;
   
   //NOTE(luis) added this to prevent mouse from clicking titlebar since we want that to select tabs for us
   b64 showing_file_bar = false;
   if(view_get_setting(app, view, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar) //draw file bar with tabs
   {
      Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
      Face_ID face_id = get_face_id(app, buffer);
      Face_Metrics metrics = get_face_metrics(app, face_id);
      f32 line_height = metrics.line_height;
      if(mouse.p.y > (line_height + 2.0f))
      {
         i64 pos = view_pos_from_xy(app, view, V2f32(mouse.p));  
         view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
         view_set_mark(app, view, seek_pos(pos));   
      }   
      else //detect if we are hitting a tab 
      {
         Buffer_Tab_Group *group = view_get_tab_group(app, view);
         if(group)
         {
            Scratch_Block scratch(app);
            Rect_f32 view_rect = view_get_screen_rect(app, view);
            f32 atx = view_rect.x0;
            for(i32 tab_index = 0; tab_index < group->tab_count; tab_index += 1)
            {
               Fancy_Line line = {};
               Buffer_ID tab = group->tabs[tab_index];
               b32 is_current_tab = tab_index == group->current_tab;
               add_fancy_strings_for_tab(app, &line, scratch, tab, is_current_tab);
               
               f32 width = get_fancy_line_width(app, face_id, &line);
               if(width > 0)
               {
                  if(mouse.p.x >= atx && mouse.p.x < (atx + width))
                  {
                     ON_MOUSE_CLICK_TAB_INDEX = tab_index;
                     group->current_tab = tab_index;
                     //set_new_current_tab(app, state, tab_index);
                     break;
                  }                     
               }
               atx += width;
            }   
         }
      }
   }
   else
   {
      //what we normally did
      i64 pos = view_pos_from_xy(app, view, V2f32(mouse.p));  
      view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
      view_set_mark(app, view, seek_pos(pos));
   }
   
}

//on mouse drag
CUSTOM_COMMAND_SIG(luis_mouse_drag)
CUSTOM_DOC("If the mouse left button is pressed, sets the cursor position to the mouse position.")
{
   View_ID view = get_active_view(app, Access_ReadVisible);
   Mouse_State mouse = get_mouse_state(app);
   
   if(mouse.l)
   {
      if(ON_MOUSE_CLICK_TAB_INDEX != -1) //we clicked on a tab
      {
         no_mark_snap_to_cursor(app, view);
         Buffer_Tab_Group *group = view_get_tab_group(app, view);
         if(group)
         {
            Buffer_ID buffer = view_get_buffer(app, view, Access_Always);
            Face_ID face_id = get_face_id(app, buffer);
            Scratch_Block scratch(app);
            Rect_f32 view_rect = view_get_screen_rect(app, view);
            f32 atx = view_rect.x0;
            //find tab_index that isn't mouse selected one, and swap it with the selected one
            for(i32 tab_index = 0; tab_index < group->tab_count; tab_index += 1)
            {
               Fancy_Line line = {};
               b32 is_current_tab = tab_index == group->current_tab;
               add_fancy_strings_for_tab(app, &line, scratch, group->tabs[tab_index], is_current_tab);
               
               f32 width = get_fancy_line_width(app, face_id, &line);
               //if(tab_index == MOUSE_DRAG_LAST_SWAPPED_TAB_INDEX &&
               if(tab_index != ON_MOUSE_CLICK_TAB_INDEX && //tab_index != MOUSE_DRAG_LAST_SWAPPED_TAB_INDEX 
                  width > 0)
               {
                  f32 min = atx + width*0.33f;
                  f32 max = atx + width - width*0.33f;
                  if(mouse.p.x >= min && mouse.p.x < max)
                  {
                     Swap(Buffer_ID, group->tabs[tab_index], group->tabs[ON_MOUSE_CLICK_TAB_INDEX]);
                     group->current_tab = tab_index;
                     //set_new_current_tab(app, state, tab_index);
                     //MOUSE_DRAG_LAST_SWAPPED_TAB_INDEX = ON_MOUSE_CLICK_TAB_INDEX; 
                     ON_MOUSE_CLICK_TAB_INDEX = tab_index;
                     break;
                  }                     
               }
               atx += width;
            }   
         }
      }
      //original 4coder command
      else  
      {
         i64 pos = view_pos_from_xy(app, view, V2f32(mouse.p));
         view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
         no_mark_snap_to_cursor(app, view);
         set_next_rewrite(app, view, Rewrite_NoChange);
      }   
   } 
   else if(mouse.release_l && ON_MOUSE_CLICK_TAB_INDEX != -1)
   {
      ON_MOUSE_CLICK_TAB_INDEX = -1;
   }
}

CUSTOM_COMMAND_SIG(luis_mouse_release)
CUSTOM_DOC("Sets the cursor position to the mouse position.")
{
   if(ON_MOUSE_CLICK_TAB_INDEX != -1)
   {
      ON_MOUSE_CLICK_TAB_INDEX = -1;
      //MOUSE_DRAG_LAST_SWAPPED_TAB_INDEX = -1;   
   }
   else
   {
      View_ID view = get_active_view(app, Access_ReadVisible);
      Mouse_State mouse = get_mouse_state(app);
      i64 pos = view_pos_from_xy(app, view, V2f32(mouse.p));
      view_set_cursor_and_preferred_x(app, view, seek_pos(pos));
      no_mark_snap_to_cursor(app, view);   
   }
}