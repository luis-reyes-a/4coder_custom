internal void
view_new_tab_group(Application_Links *app, View_ID view)
{
   Managed_Scope scope = view_get_managed_scope(app, view);
   i32 *tab_group_index = scope_attachment(app, scope, view_tab_group_index, i32);
   if(tab_group_index && BUFFER_TAB_GROUP_COUNT < countof(BUFFER_TAB_GROUPS))
   {
      foreach_index_inc(i, countof(BUFFER_TAB_GROUPS))
      {
         Buffer_Tab_Group *group = BUFFER_TAB_GROUPS + i; 
         if(group->tab_count == 0)
         {
            *tab_group_index = i;
            *group = {};
            group->tabs[0] = view_get_buffer(app, view, Access_Always);
            group->tab_count = 1;
            BUFFER_TAB_GROUP_COUNT += 1;
            return;
         }
      }
   }   
}

CUSTOM_COMMAND_SIG(view_new_tab_group)
CUSTOM_DOC("make a new tab group")
{
   View_ID view = get_active_view(app, Access_Always);
   view_new_tab_group(app, view);
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
{	luis_offset_tab(app, -1)	}

CUSTOM_COMMAND_SIG(luis_tab_next)
CUSTOM_DOC("move next tab")
{	luis_offset_tab(app, 1)	}
   

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
      for(Code_Index_Note *n = first_note_in_hash; n; n = n->next_in_hash)
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
            view_set_buffer(); //TODO!
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
   i64 linenum = get_line_number_from_pos(app, view_get_buffer(app, view, Access_Always), view_get_cursor_pos(app));
   Range_i64 range = get_visual_line_start_end_pos(app, view, linenum);
   view_set_cursor_and_preferred_x(app, view, seek_pos(range.min));
}

CUSTOM_COMMAND_SIG(luis_end)
CUSTOM_DOC("go end of visual line")
{
   View_ID view = get_active_view(app, Access_Always);
   i64 linenum = get_line_number_from_pos(app, view_get_buffer(app, view, Access_Always), view_get_cursor_pos(app));
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
   //TODO
}
luis_fsearch //fuck me
luis_rsearch //fuck me

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

CUSTOM_COMMAND_SIG(luis_scope_braces)
CUSTOM_DOC("writes {}")
{
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
   if(strmatch(SCu8("struct "), line, 7) ||
      strmatch(SCu8("enum "),   line, 5) ||
      strmatch(SCu8("union "),  line, 6))
   {
      string.str = (u8 *)"\n{\n\n};";
      string.size = sizeof("\n{\n\n};") - 1;
   }
   //else if(strmatch(SCu8("case "), line, 5))
   //{
   //string.str = (u8 *)":\n{\n\n} break;";
   //string.size = sizeof(":\n{\n\n} break;") - 1;
   //}
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
}