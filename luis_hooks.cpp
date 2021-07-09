function void
luis_4coder_initialize(Application_Links *app, String_Const_u8_Array file_names, i32 override_font_size, b32 override_hinting){
#define M \
"Welcome to " VERSION "\n" \
"If you're new to 4coder there is a built in tutorial\n" \
"Use the key combination [ X Alt ] (on mac [ X Control ])\n" \
"Type in 'hms_demo_tutorial' and press enter\n" \
"\n" \
"Direct bug reports and feature requests to https://github.com/4coder-editor/4coder/issues\n" \
"\n" \
"Other questions and discussion can be directed to editor@4coder.net or 4coder.handmade.network\n" \
"\n" \
"The change log can be found in CHANGES.txt\n" \
"\n"
   print_message(app, string_u8_litexpr(M));
#undef M
   
   Scratch_Block scratch(app);
   
   load_config_and_apply(app, &global_config_arena, override_font_size, override_hinting);
   
   SMALL_CODE_FACE = RION_load_face_id(app, SCu8("consola.ttf"),  -2);
   ITALICS_CODE_FACE = RION_load_face_id(app, SCu8("consolai.ttf"), 0);
   BOLD_CODE_FACE    = RION_load_face_id(app, SCu8("consolab.ttf"), 0);
   
   String_Const_u8 bindings_file_name = string_u8_litexpr("bindings.4coder");
   String_Const_u8 mapping = def_get_config_string(scratch, vars_save_string_lit("mapping"));
   
   if (string_match(mapping, string_u8_litexpr("mac-default"))){
      bindings_file_name = string_u8_litexpr("mac-bindings.4coder");
   }
   else if (OS_MAC && string_match(mapping, string_u8_litexpr("choose"))){
      bindings_file_name = string_u8_litexpr("mac-bindings.4coder");
   }
   
   // TODO(allen): cleanup
   String_ID global_map_id = vars_save_string_lit("keys_global");
   String_ID file_map_id = vars_save_string_lit("keys_file");
   String_ID code_map_id = vars_save_string_lit("keys_code");
   String_ID modal_map_id = vars_save_string_lit("keys_modal");
   
   if (dynamic_binding_load_from_file(app, &framework_mapping, bindings_file_name)){
      luis_essential_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id, modal_map_id);
   }
   else{
      luis_essential_mapping(&framework_mapping, global_map_id, file_map_id, code_map_id, modal_map_id);
      setup_built_in_mapping(app, mapping, &framework_mapping, global_map_id, file_map_id, code_map_id);
   }
   
   // open command line files
   String_Const_u8 hot_directory = push_hot_directory(app, scratch);
   for (i32 i = 0; i < file_names.count; i += 1){
      Temp_Memory_Block temp(scratch);
      String_Const_u8 input_name = file_names.vals[i];
      String_Const_u8 full_name = push_u8_stringf(scratch, "%.*s/%.*s",
                                                  string_expand(hot_directory),
                                                  string_expand(input_name));
      Buffer_ID new_buffer = create_buffer(app, full_name, BufferCreate_NeverNew|BufferCreate_MustAttachToFile);
      if (new_buffer == 0){
         create_buffer(app, input_name, 0);
      }
   }
}

CUSTOM_COMMAND_SIG(luis_startup)
CUSTOM_DOC("Default command for responding to a startup event")
{
   ProfileScope(app, "default startup");
   User_Input input = get_current_input(app);
   if (match_core_code(&input, CoreCode_Startup)){
      String_Const_u8_Array file_names = input.event.core.file_names;
      load_themes_default_folder(app);
      Face_Description description = get_face_description(app, 0);
      luis_4coder_initialize(app, file_names, description.parameters.pt_size, description.parameters.hinting);
      View_ID view = get_active_view(app, Access_Always);
      //Buffer_ID default_buffer_id = buffer_identifier_to_id(app, buffer_identifier(string_u8_litexpr("*scratch*")));
      Buffer_ID default_buffer_id = buffer_identifier_to_id(app, buffer_identifier(string_u8_litexpr("*messages*")));
      view_set_buffer(app, view, default_buffer_id, 0);
      luis_new_tab_group(app);
      
      b32 auto_load = def_get_config_b32(vars_save_string_lit("automatically_load_project"));
      if (auto_load){
         load_project(app);
      }
   }
   
   {
      def_audio_init();
#if 0
      Scratch_Block scratch(app);
      FILE *file = def_search_normal_fopen(scratch, "audio_test/raygun_zap.wav", "rb");
      if (file != 0){
         Audio_Clip test_clip = audio_clip_from_wav_FILE(&global_permanent_arena, file);
         fclose(file);
         
         local_persist Audio_Control test_control = {};
         test_control.channel_volume[0] = 1.f;
         test_control.channel_volume[1] = 1.f;
         def_audio_play_clip(test_clip, &test_control);
      }
#endif
   }
   
   {
      def_enable_virtual_whitespace = def_get_config_b32(vars_save_string_lit("enable_virtual_whitespace"));
      clear_all_layouts(app);
   }
}



CUSTOM_COMMAND_SIG(luis_view_input_handler)
CUSTOM_DOC("Input consumption loop for default view behavior")
{
   Scratch_Block scratch(app);
   default_input_handler_init(app, scratch);
   
   View_ID view = get_this_ctx_view(app, Access_Always);
   Managed_Scope scope = view_get_managed_scope(app, view);
   //View_ID active_view = get_active_view(app, Access_Always);
   
   for (;;){
      // NOTE(allen): Get input
      User_Input input = get_next_input(app, EventPropertyGroup_Any, 0);
      if (input.abort){
         break;
      }
      
      ProfileScopeNamed(app, "before view input", view_input_profile);
      
      // NOTE(allen): Mouse Suppression
      Event_Property event_properties = get_event_properties(&input.event);
      if (suppressing_mouse && (event_properties & EventPropertyGroup_AnyMouseEvent) != 0){
         continue;
      }
      
      // NOTE(allen): Get binding
      if (implicit_map_function == 0){
         implicit_map_function = default_implicit_map;
      }
      Implicit_Map_Result map_result = implicit_map_function(app, 0, 0, &input.event);
      if (map_result.command == 0){
         leave_current_input_unhandled(app);
         continue;
      }
      
      // NOTE(allen): Run the command and pre/post command stuff
      b32 actually_do_command = true;
      b32 try_to_recover_peek_view_after_command = false;
      { //NOTE(luis) pre command
         Rewrite_Type *next_rewrite = scope_attachment(app, scope, view_next_rewrite_loc, Rewrite_Type);
         *next_rewrite = Rewrite_None;
         
         Custom_Command_Function *cmd = map_result.command;
         if(cmd == open_panel_vsplit || cmd == open_panel_hsplit)
         {
            if(luis_view_has_flags(app, view, VIEW_IS_PEEK_WINDOW))
               actually_do_command = false;
            else 
            {
               View_ID peek = luis_get_peek_window(app, view);
               if(peek)
               {
                  view_close(app, peek);
                  try_to_recover_peek_view_after_command = true;   
               }
            }
         }
      }
      
      ProfileCloseNow(view_input_profile);
      if(actually_do_command)
      {
         //b32 do_kill_tab_group = luis_view_has_flags(app, view, VIEW_KILL_TAB_GROUP_ON_VIEW_CLOSE);
         //i32 tab_group_index = view_get_tab_group_index(app, view);
         map_result.command(app);
         //if(!view_exists(app, view) && do_kill_tab_group) //pseudo-hook for when a view is being destroyed
         //{
            //kill_tab_group(app, tab_group_index);
         //}
      }
         
      if(try_to_recover_peek_view_after_command)
      {
         Peek_Code_Index_State *state = scope_attachment(app, scope, view_code_peek_state, Peek_Code_Index_State);
         if(state)
         {
            peek_next_code_index(app, view, state, state->index);
            view_set_active(app, view); //peek_next_code_index sets peek active, so we just undo it... yeah it's messy
            //luis_change_active_panel_ignore_peek_windows(app) 
         }
      }
      
      ProfileScope(app, "after view input");
      
      { //NOTE(luis) post command
         Rewrite_Type *next_rewrite = scope_attachment(app, scope, view_next_rewrite_loc, Rewrite_Type);
         if (next_rewrite != 0){
            if (*next_rewrite != Rewrite_NoChange){
               Rewrite_Type *rewrite =
                  scope_attachment(app, scope, view_rewrite_loc, Rewrite_Type);
               *rewrite = *next_rewrite;
            }
         }
         
         View_ID current_active_view = get_active_view(app, Access_Always);
         
         if (fcoder_mode == FCoderMode_NotepadLike && (view == current_active_view))
         {
            b32 snap_mark_to_cursor = true;
            if(luis_view_has_flags(app, view, VIEW_NOTEPAD_MODE_MARK_SET))
            {
               snap_mark_to_cursor = false;
               Custom_Command_Function *cmd = map_result.command;
               if(cmd == cut || cmd == copy || cmd == paste || cmd == paste_and_indent || cmd == backspace_char ||
                  cmd == write_text_input || cmd == write_space || cmd == luis_write_underscore || cmd == luis_write_pointer_arrow ||
                  cmd == luis_write_tab || cmd == luis_write_newline || cmd == write_text_and_auto_indent ||
                  cmd == auto_indent_line_at_cursor || cmd == auto_indent_whole_file || cmd == auto_indent_range ||
                  cmd == delete_range || cmd == luis_multiline_comment_toggle || cmd == place_in_scope || cmd == luis_surround_in_parens ||
                  cmd == view_buffer_other_panel || cmd == if_read_only_goto_position || cmd == if_read_only_goto_position_same_panel || cmd == luis_escape)
               {
                  luis_view_clear_flags(app, view, VIEW_NOTEPAD_MODE_MARK_SET);
                  snap_mark_to_cursor = true;
               }   
            }
            if(snap_mark_to_cursor)
            {
               i64 pos = view_get_cursor_pos(app, view);
               view_set_mark(app, view, seek_pos(pos));
            }
         }
      }
   }
}

function void //adpated from similar function in 4coder_draw.cpp
luis_draw_character_block_outline(Application_Links *app, Text_Layout_ID layout, Range_i64 range, f32 roundness, FColor fcolor){
   if (range.first < range.one_past_last){
      i64 i = range.first;
      Rect_f32 first_rect = text_layout_character_on_screen(app, layout, i);
      i += 1;
      Range_f32 y = rect_range_y(first_rect);
      Range_f32 x = rect_range_x(first_rect);
      for (;i < range.one_past_last; i += 1){
         Rect_f32 rect = text_layout_character_on_screen(app, layout, i);
         if (rect.x0 < rect.x1 && rect.y0 < rect.y1){
            Range_f32 new_y = rect_range_y(rect);
            Range_f32 new_x = rect_range_x(rect);
            b32 joinable = false;
            if (new_y == y && (range_overlap(x, new_x) || x.max == new_x.min || new_x.max == x.min)){
               joinable = true;
            }
            
            if (!joinable){
               //draw_rectangle(app, Rf32(x, y), roundness, fcolor_resolve(fcolor));
               draw_rectangle_outline_fcolor(app, Rf32(x, y), roundness, 4.0f, fcolor);
               y = new_y;
               x = new_x;
            }
            else{
               x = range_union(x, new_x);
            }
         }
      }
      draw_rectangle_outline_fcolor(app, Rf32(x, y), roundness, 4.0f, fcolor);
   }
}

internal b32
does_token_look_like_var_identifier(Application_Links *app, Buffer_ID buffer, Token_Iterator_Array *it, Token *identifier)
{
   b32 looks_like_var = false;
   if(identifier->kind == TokenBaseKind_Identifier)
   {
      //NOTE(luis) this won't handle macros even markup one's that expand to nothing
      for(Token *token = identifier - 1; token >= it->tokens; token -= 1)
      {
         if(token->kind == TokenBaseKind_Whitespace) ; //keep going
         else if(token->kind == TokenBaseKind_Operator && token->sub_kind == TokenCppKind_Star) ; //keep going
         else if(token->kind == TokenBaseKind_Identifier) //stop, see if it looks like a note type
         {
            Scratch_Block scratch(app);
            String_Const_u8 string = push_token_lexeme(app, scratch, buffer, token);
            Code_Index_Note *note = code_index_note_from_string(string);
            looks_like_var = note && (note->note_kind == CodeIndexNote_Type);
            break;
         }
         else if(token->kind == TokenBaseKind_Keyword) //stop, see if it's a built-in keyword type
         {
            looks_like_var = (token->sub_kind == TokenCppKind_Void ||
                              token->sub_kind == TokenCppKind_Bool ||
                              token->sub_kind == TokenCppKind_Char ||
                              token->sub_kind == TokenCppKind_Int ||
                              token->sub_kind == TokenCppKind_Float ||
                              token->sub_kind == TokenCppKind_Double ||
                              token->sub_kind == TokenCppKind_Long ||
                              token->sub_kind == TokenCppKind_Short);
            break;
         }
         else break; //didn't expect that token type in a var decl token sequence, just end
         
      }   
   }
   return looks_like_var;
}

struct Token_Visual_Properties
{
   ARGB_Color color;
   b32 underline;
   Face_ID special_face_id;
};

internal Token_Visual_Properties
get_token_visual_properties(Application_Links *app, Buffer_ID buffer, Token_Iterator_Array *it, Token *token)
{
   Scratch_Block scratch(app);
   Token_Visual_Properties prop = {};
   prop.color = fcolor_resolve(get_token_color_cpp(*token));
   Code_Index_Note *note = code_index_note_from_string(push_token_lexeme(app, scratch, buffer, token));
   if(note && note->note_kind == CodeIndexNote_Function)   prop.color = fcolor_resolve(fcolor_id(luiscolor_function));//= 0xff8CD0D3; //cyan
   else if(note && note->note_kind == CodeIndexNote_Type)  prop.color = fcolor_resolve(fcolor_id(luiscolor_type));//= 0xffE89393; //pinkish
   else if(note && note->note_kind == CodeIndexNote_Macro) prop.color = fcolor_resolve(fcolor_id(luiscolor_macro));//= 0xffDFAF8F; //orange
   else if(does_token_look_like_var_identifier(app, buffer, it, token)) prop.color = fcolor_resolve(fcolor_id(luiscolor_variable_decl));
   
   if(token->kind == TokenBaseKind_Keyword)      prop.special_face_id = BOLD_CODE_FACE;
   else if(token->kind == TokenBaseKind_Comment) prop.special_face_id = ITALICS_CODE_FACE;
   else if(token->kind == TokenBaseKind_Preprocessor) prop.underline = true;
   //else if(token->kind == TokenBaseKind_StatementClose && token->sub_kind == TokenCppKind_Comma) 
   //{
   //prop.color = fcolor_resolve(fcolor_id(defcolor_keyword));
   //prop.special_face_id = bold_code_face;
   //}
   //else if(does_token_look_like_var_identifier(app, buffer, &it, token)) special_face_id = bold_code_face;
   
   
   return prop;
}

internal void
draw_buffer_range(Application_Links *app, Buffer_ID buffer, Text_Layout_ID layout_id, Face_ID face_id, Range_i64 range, ARGB_Color color)
{
   //stupid easy way I could get this to work.... you should probably scan for words to reduce draw_string calls, but whatevs
   Scratch_Block scratch(app);
   String_Const_u8 string = push_buffer_range(app, scratch, buffer, range);
   for(i64 byte_pos = range.min; byte_pos < range.max; byte_pos += 1) 
   {
      Vec2_f32 pos = text_layout_character_on_screen(app, layout_id, byte_pos).p0;
      String_Const_u8 c;
      c.str = string.str + (byte_pos - range.min);
      c.size = 1;
      draw_string(app, face_id, c, pos, color);
   }
}

internal void
draw_string_upright(Application_Links *app, Face_ID face_id, ARGB_Color color, String_Const_u8 str, Vec2_f32 pos)
{
   f32 angle = pi_f32 * 2 * 0.75f;
   draw_string_oriented(app, face_id, color, str, pos, 0, V2f32(cos_f32(angle), sin_f32(angle)));   
}

//adapted from 4coder_fleury, thanks RION!
internal void
draw_brace_lines(Application_Links *app, View_ID view_id, Buffer_ID buffer, Text_Layout_ID text_layout_id, Range_i64 range, Rect_f32 rect)
{
   //a bit of an overkill way to test this, we could compare range_start_rect and range_end_rect, but whatever... 
   i64 range_start_linenum = get_line_number_from_pos(app, buffer, range.start);
   i64 range_end_linenum   = get_line_number_from_pos(app, buffer, range.end);
   if(range_end_linenum == range_start_linenum) return; 
   
   Scratch_Block scratch(app);
   Face_ID face_id = get_face_id(app, buffer);
   Face_Metrics metrics = get_face_metrics(app, face_id);
   Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
   
   u32 color = fcolor_resolve(fcolor_id(defcolor_text_default));
   Rect_f32 line_number_rect = {};
   b32 show_line_number_margins = def_get_config_b32(vars_save_string_lit("show_line_number_margins"));
   if(show_line_number_margins)
   {
      Rect_f32 rect = view_get_screen_rect(app, view_id);
      
      Face_Metrics face_metrics = get_face_metrics(app, face_id);
      f32 digit_advance = face_metrics.decimal_digit_advance;
      
      Rect_f32_Pair pair = layout_line_number_margin(app, buffer, rect, digit_advance);
      line_number_rect = pair.min;
      line_number_rect.x1 += 4;
   }  
   f32 x_offset = view_get_screen_rect(app, view_id).x0 + 6 - view_get_buffer_scroll(app, view_id).position.pixel_shift.x + (line_number_rect.x1 - line_number_rect.x0);
   
   
   f32 x_position = 0.0f;
   Rect_f32 range_start_rect = text_layout_character_on_screen(app, text_layout_id, range.start);
   Rect_f32 range_end_rect   = text_layout_character_on_screen(app, text_layout_id, range.end);
   
   
   String_Const_u8 line = push_buffer_line(app, scratch, buffer, range_end_linenum);
   for(u64 char_idx = 0; char_idx < line.size; char_idx += 1)
   {
#if 0
      if(!character_is_whitespace(line.str[char_idx]))
      {
         x_position = metrics.space_advance * char_idx;
         break;
      }
#else
      if(line.str[char_idx] == ' ') x_position += metrics.space_advance;
      else if(line.str[char_idx] == '\t') x_position += 3*metrics.space_advance;
      else break;
#endif
   }
   
   float y_start = rect.y0;
   float y_end = rect.y1;
   if(range.start >= visible_range.start)
   {
      y_start = range_start_rect.y0 + metrics.line_height;
   }
   if(range.end <= visible_range.end)
   {
      y_end = range_end_rect.y0;
   }
   
   Rect_f32 line_rect = {0};
   line_rect.x0 = x_position + x_offset;
   line_rect.x1 = line_rect.x0 + 2;
   line_rect.y0 = y_start;
   line_rect.y1 = y_end;
   
   draw_rectangle(app, line_rect, 0.5f, color);
   
   if(!SHOW_BRACE_LINE_ANNOTATIONS) return;   
   //brace annotation
   Token_Array token_array = get_token_array_from_buffer(app, buffer);
   Token_Iterator_Array it = token_iterator_pos(0, &token_array, range.start);
   Token *start_token = 0;
   Token *end_token = 0;
   //i64 token_count = 0;
   {
      end_token = token_it_read(&it);
      
      int paren_nest = 0;
      for(;;)
      {
         if(!token_it_dec_non_whitespace(&it)) break;
         Token *token = token_it_read(&it);
         if(token)
         {
            //token_count += 1;
            
            if(token->kind == TokenBaseKind_ParentheticalClose)
            {
               ++paren_nest;
            }
            else if(token->kind == TokenBaseKind_ParentheticalOpen)
            {
               --paren_nest;
            }
            else if(paren_nest == 0 &&
                    (token->kind == TokenBaseKind_ScopeClose ||
                     (token->kind == TokenBaseKind_StatementClose && token->sub_kind != TokenCppKind_Colon)))
            {
               break;
            }
            else if((token->kind == TokenBaseKind_Identifier || token->kind == TokenBaseKind_Keyword /*|| token->kind == TokenBaseKind_Comment*/) &&
                    !paren_nest)
            {
               start_token = token;
               break;
            }
            
         }
         else break;
      }
   }
   
   // NOTE(rjf): Draw.
   if(start_token)// && start_token->pos < visible_range.min)
   {
      metrics = get_face_metrics(app, SMALL_CODE_FACE);
      f32 advance = metrics.normal_advance;
      i32 num_characters_can_draw = (i32)((y_end - y_start) / advance);
      
      Token *token = start_token;
      Vec2_f32 pos = line_rect.p1;
      //pos.y = lerp(line_rect.p0.y, 0.5f, line_rect.p1.y);
      //pos.y += (string.size * advance)*0.5f;
      while((num_characters_can_draw > 0) && (token != end_token))
      {
         Token_Visual_Properties prop = get_token_visual_properties(app, buffer, &it, token);
         //String_Const_u8 string = push_buffer_line(app, scratch, buffer, get_line_number_from_pos(app, buffer, start_token->pos));
         String_Const_u8 string = push_token_lexeme(app, scratch, buffer, token);
         if(token->kind == TokenBaseKind_Whitespace) string.size = 1;
         //string = string_skip_chop_whitespace(string);
         // NOTE(rjf): Special case to handle CRLF-newline files.
         if(string.str[string.size - 1] == 13)
         {
            string.size -= 1;
         }
         
         string.size = Min(string.size, num_characters_can_draw);
         draw_string_upright(app, SMALL_CODE_FACE, prop.color, string, pos);
         pos.y -= string.size*advance;
         num_characters_can_draw -= (i32)string.size;
         token += 1;
      }
   }
}

internal b32
draw_paren_range_and_its_sub_ranges(Application_Links *app, Buffer_ID buffer, Text_Layout_ID text_layout_id, Range_i64 max_range, i32 color_index)
{
   b32 drew_range_and_subranges = false;
   Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
   if(range_overlap(max_range, visible_range))
   {
      Color_Array color_array = finalize_color_array(defcolor_text_cycle);
      ARGB_Color color = color_array.vals[color_index % color_array.count];
      paint_text_color_pos(app, text_layout_id, max_range.min,   color);
      paint_text_color_pos(app, text_layout_id, max_range.max-1, color);
      
      i32 range_count = 0;
      Range_i64 ranges[16];
      if(find_next_parens_absolute(app, buffer, max_range.min, ranges + 0) &&
         range_overlap(ranges[0], max_range) && range_overlap(ranges[0], visible_range)) //find first sub range
      {
         drew_range_and_subranges = true;
         range_count = 1;
         while(range_count < ArrayCount(ranges) && 
               find_next_parens_absolute(app, buffer, ranges[range_count-1].max-1, ranges + range_count))
         {
            if(range_overlap(ranges[range_count], max_range) && range_overlap(ranges[range_count], visible_range))
               range_count += 1;
            else break;
         }
         
         for(i32 i = 0; i < range_count; i += 1)
            draw_paren_range_and_its_sub_ranges(app, buffer, text_layout_id, ranges[i], color_index + 1);
      }
      
#if 1 //draw commas to match the max_range color we chose
      Token_Array token_array = get_token_array_from_buffer(app, buffer);
      Token_Iterator_Array it = token_iterator_pos(0, &token_array, max_range.min + 1);
      for(;;)
      {
         Token *token = token_it_read(&it);
         if(!token) break;
         if(token->pos >= (max_range.max-1)) break;
         if(token->pos >= visible_range.max) break;
         
         
         if((token->kind == TokenBaseKind_StatementClose && token->sub_kind == TokenCppKind_Comma))
            //token->kind == TokenBaseKind_Operator)
         {
            b32 in_another_subrange = false;
            for(i32 i = 0; i < range_count; i += 1)
            {
               if(token->pos > ranges[i].min && token->pos < ranges[i].max)
               {
                  in_another_subrange = true;
                  break;
               }
            }
            
            if(!in_another_subrange)
               paint_text_color(app, text_layout_id, Ii64_size(token->pos, token->size), color);
         }
         if(!token_it_inc_all(&it))	break;
      }
#endif
   }
   return drew_range_and_subranges;
}

function void
luis_render_buffer(Application_Links *app, View_ID view_id, Face_ID face_id,
                   Buffer_ID buffer, Text_Layout_ID text_layout_id,
                   Rect_f32 rect)
{
   ProfileScope(app, "render buffer");
   
   View_ID active_view = get_active_view(app, Access_Always);
   b32 is_active_view = (active_view == view_id);
   Rect_f32 prev_clip = draw_set_clip(app, rect);
   
   Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
   
   // NOTE(allen): Cursor shape
   Face_Metrics metrics = get_face_metrics(app, face_id);
   u64 cursor_roundness_100 = def_get_config_u64(app, vars_save_string_lit("cursor_roundness"));
   f32 cursor_roundness = metrics.normal_advance*cursor_roundness_100*0.01f;
   f32 mark_thickness = (f32)def_get_config_u64(app, vars_save_string_lit("mark_thickness"));
   
   i64 cursor_pos = view_correct_cursor(app, view_id);
   view_correct_mark(app, view_id);
   
   // NOTE(allen): Line highlight
   b32 highlight_line_at_cursor = def_get_config_b32(vars_save_string_lit("highlight_line_at_cursor"));
   if (highlight_line_at_cursor && is_active_view){
      i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
      draw_line_highlight(app, text_layout_id, line_number, fcolor_id(defcolor_highlight_cursor_line));
   }
   
   // NOTE(allen): Token colorizing
   Token_Array token_array = get_token_array_from_buffer(app, buffer);
   if (token_array.tokens != 0)
   {
#if 0
      draw_cpp_token_colors(app, text_layout_id, &token_array);
#else
      Scratch_Block scratch(app);
      i64 first_index = token_index_from_pos(&token_array, visible_range.first);
      Token_Iterator_Array it = token_iterator_index(0, &token_array, first_index);
      for(;;)
      {
         Token *token = token_it_read(&it);
         if(token->pos >= visible_range.one_past_last)	break;
         Token_Visual_Properties prop = get_token_visual_properties(app, buffer, &it, token);
         Rect_f32 first_rect = text_layout_character_on_screen(app, text_layout_id, token->pos);
         if(prop.special_face_id)
            draw_buffer_range(app, buffer, text_layout_id, prop.special_face_id, Ii64_size(token->pos, token->size), prop.color);
         else paint_text_color(app, text_layout_id, Ii64_size(token->pos, token->size), prop.color);
         
         
         String_Const_u8 underline_str = SCu8("_______________________________________________________________________________________________________________________________________________________________________________________________");
         if(prop.underline && (u64)token->size <= underline_str.size)
         {
            underline_str.size = (u64)token->size;
            draw_string(app, face_id, underline_str, first_rect.p0, prop.color);
         }
         
         if (!token_it_inc_all(&it))	break;
      }
#endif
      
      // NOTE(allen): Scan for TODOs and NOTEs
      b32 use_comment_keyword = def_get_config_b32(vars_save_string_lit("use_comment_keyword"));
      if (use_comment_keyword){
         Comment_Highlight_Pair pairs[] = {
            {string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0)},
            {string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1)},
         };
         draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs, ArrayCount(pairs));
      }
      
      b32 do_draw_brace_lines = true;
      if(do_draw_brace_lines)
      {
         Range_i64 range;
         b32 found_next_scope = find_maximal_scope(app, buffer, visible_range.min, &range);
         if(!found_next_scope && find_next_scope_absolute(app, buffer, visible_range.min, &range))
            found_next_scope = range_overlap(range, visible_range);
         
         while(found_next_scope)
         {
            if(range_overlap(range, visible_range)) 
               draw_brace_lines(app, view_id, buffer, text_layout_id, range, rect);
            found_next_scope = find_next_scope_absolute(app, buffer, range.min+1, &range);
            if(found_next_scope && range.min > visible_range.max) break;
         }
      }
      
      b32 do_draw_colored_parens = true;
      if(do_draw_colored_parens)
      {
         //get the first max_parens
         Range_i64 max_range;
         if(find_maximal_parens(app,       buffer, visible_range.min, &max_range) ||
            find_next_parens_absolute(app, buffer, visible_range.min, &max_range))
         {
            while(range_overlap(max_range, visible_range))
            {
               draw_paren_range_and_its_sub_ranges(app, buffer, text_layout_id, max_range, 0);
               b32 found_next = find_next_parens_absolute(app, buffer, max_range.max-1, &max_range);
               if(!found_next) break;
            }
         }
      }
   }
   else{
      paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
   }
   
   
   
   // NOTE(allen): Scope highlight
   b32 use_scope_highlight = def_get_config_b32(vars_save_string_lit("use_scope_highlight"));
   if (use_scope_highlight){
      Color_Array colors = finalize_color_array(defcolor_back_cycle);
      draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
   }
   
   b32 use_error_highlight = def_get_config_b32(vars_save_string_lit("use_error_highlight"));
   b32 use_jump_highlight = def_get_config_b32(vars_save_string_lit("use_jump_highlight"));
   if (use_error_highlight || use_jump_highlight){
      // NOTE(allen): Error highlight
      String_Const_u8 name = string_u8_litexpr("*compilation*");
      Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
      if (use_error_highlight){
         draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer,
                              fcolor_id(defcolor_highlight_junk));
      }
      
      // NOTE(allen): Search highlight
      if (use_jump_highlight){
         Buffer_ID jump_buffer = get_locked_jump_buffer(app);
         if (jump_buffer != compilation_buffer){
            draw_jump_highlights(app, buffer, text_layout_id, jump_buffer,
                                 fcolor_id(defcolor_highlight_white));
         }
      }
   }
   
   // NOTE(allen): Color parens
   b32 use_paren_helper = def_get_config_b32(vars_save_string_lit("use_paren_helper"));
   if (use_paren_helper){
      Color_Array colors = finalize_color_array(defcolor_text_cycle);
      draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
   }
   
   // NOTE(allen): Whitespace highlight
   b64 show_whitespace = false;
   view_get_setting(app, view_id, ViewSetting_ShowWhitespace, &show_whitespace);
   if (show_whitespace){
      if (token_array.tokens == 0){
         draw_whitespace_highlight(app, buffer, text_layout_id, cursor_roundness);
      }
      else{
         draw_whitespace_highlight(app, text_layout_id, &token_array, cursor_roundness);
      }
   }
   
   // NOTE(luis): draw cursor
   if(fcoder_mode == FCoderMode_NotepadLike)
   {
      b32 has_highlight_range = draw_highlight_range(app, view_id, buffer, text_layout_id, cursor_roundness);
      if (!has_highlight_range)
      {
         i32 cursor_sub_id = default_cursor_sub_id();
         
         //i64 cursor_pos = view_get_cursor_pos(app, view_id);
         i64 mark_pos = view_get_mark_pos(app, view_id);
         if (is_active_view){
            //draw selection range if any...
            if(fcoder_mode == FCoderMode_NotepadLike && cursor_pos != mark_pos)
            {
               Range_i64 range = Ii64(cursor_pos, mark_pos);
               luis_draw_character_block_outline(app, text_layout_id, range, metrics.normal_advance*50*0.01f, fcolor_id(defcolor_highlight));
            }
            //mark block and character over
            FColor cursor_color = fcolor_id(defcolor_cursor, cursor_sub_id);
            FColor mark_color   = fcolor_id(defcolor_mark);
            if(IN_MODAL_MODE) cursor_color = fcolor_id(luiscolor_modal_cursor);//fcolor_id(defcolor_highlight);
            draw_character_block(app, text_layout_id, mark_pos, cursor_roundness, mark_color);
            draw_character_block(app, text_layout_id, cursor_pos, cursor_roundness, cursor_color);
            paint_text_color_pos(app, text_layout_id, cursor_pos,
                                 fcolor_id(defcolor_at_cursor));
            
            
         }
         else{
            draw_character_wire_frame(app, text_layout_id, mark_pos,
                                      cursor_roundness, mark_thickness,
                                      fcolor_id(defcolor_mark));
            draw_character_wire_frame(app, text_layout_id, cursor_pos,
                                      cursor_roundness, mark_thickness,
                                      fcolor_id(defcolor_cursor, cursor_sub_id));
         }
      }
   }
   else if(fcoder_mode == FCoderMode_Original)
   {
      draw_original_4coder_style_cursor_mark_highlight(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness);
   }
   
   // NOTE(allen): Fade ranges
   paint_fade_ranges(app, text_layout_id, buffer);
   
   // NOTE(allen): put the actual text on the actual screen
   draw_text_layout_default(app, text_layout_id);
   
   draw_set_clip(app, prev_clip);
}

internal void
luis_draw_file_bar(Application_Links *app, View_ID view_id, Buffer_ID buffer, Face_ID face_id, Rect_f32 bar)
{
   i32 current_tab = 0;
   Buffer_Tab_Group *group = 0;
   {
      Managed_Scope scope = view_get_managed_scope(app, view_id);
      i32 *tab_group_index = scope_attachment(app, scope, view_tab_group_index, i32);
      if(tab_group_index)
      {
         assert(*tab_group_index >= 0 && *tab_group_index < countof(BUFFER_TAB_GROUPS));
         group = BUFFER_TAB_GROUPS + *tab_group_index;
      }
      
      //i32 *current_tab_ptr = scope_attachment(app, scope, view_current_tab, i32);
      //if(current_tab_ptr)
         //current_tab = *current_tab_ptr;
   }
   
   
   if(!group) //if not available, do normal 4coder way
   {
      draw_file_bar(app, view_id, buffer, face_id, bar);
      return;
   }
   
   current_tab = group->current_tab;
   Scratch_Block scratch(app);
   draw_rectangle_fcolor(app, bar, 0.f, fcolor_id(defcolor_bar));
   
   Fancy_Line list = {};
   FColor base_color = fcolor_id(defcolor_base);
   FColor pop2_color = fcolor_id(luiscolor_function);
   b32 current_tab_is_active_buffer = false;
   for(i32 tab_index = 0; tab_index < group->tab_count; tab_index += 1)
   {
      Buffer_ID tab = group->tabs[tab_index];
      //b32 is_current_tab = tab == buffer && tab_index == group->current_tab;
      b32 is_current_tab = (tab_index == current_tab);
      if(is_current_tab && (buffer != tab))
      {
         view_set_buffer(app, view_id, tab, 0);
         buffer = view_get_buffer(app, view_id, Access_Always);
         assert(buffer == tab);
      }
         
      add_fancy_strings_for_tab(app, &list, scratch, tab, is_current_tab);
      if(is_current_tab)
         current_tab_is_active_buffer = true; 
   }
   if(!current_tab_is_active_buffer) //if the actively viewing buffer doesn't match the current tab, then we draw it last with diff color
   {
      //Assert(state->peek_tab.id == buffer);
      String_Const_u8 unique_name = push_buffer_unique_name(app, scratch, buffer);
      push_fancy_string(scratch, &list, fcolor_id(luiscolor_macro), unique_name);
      Dirty_State dirty = buffer_get_dirty_state(app, buffer);
      u8 space[3];
      String_u8 str = Su8(space, 0, 3);
      if (HasFlag(dirty, DirtyState_UnsavedChanges)) string_append(&str, string_u8_litexpr("*"));
      if (HasFlag(dirty, DirtyState_UnloadedChanges)) string_append(&str, string_u8_litexpr("!"));
      push_fancy_string(scratch, &list, pop2_color, str.string);
      push_fancy_string(scratch, &list, base_color,  SCu8(" "));
   }
   
   
   //line number
   i64 cursor_position = view_get_cursor_pos(app, view_id);
   Buffer_Cursor cursor = view_compute_cursor(app, view_id, seek_pos(cursor_position));
   push_fancy_stringf(scratch, &list, base_color, "%3.lld-", cursor.line);
   
   //line endings
   Managed_Scope scope = buffer_get_managed_scope(app, buffer);
   Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting,
                                                    Line_Ending_Kind);
   switch (*eol_setting){
      case LineEndingKind_Binary:
      push_fancy_string(scratch, &list, base_color, string_u8_litexpr("bin"));
      break;
      
      case LineEndingKind_LF:
      push_fancy_string(scratch, &list, base_color, string_u8_litexpr("lf"));
      break;
      
      
      case LineEndingKind_CRLF:
      push_fancy_string(scratch, &list, base_color, string_u8_litexpr("crlf"));
      break;
   }
   
   Vec2_f32 pos = bar.p0 + V2f32(2.f, 2.f);
   draw_fancy_line(app, face_id, fcolor_zero(), &list, pos);   
}

function void
luis_render_caller(Application_Links *app, Frame_Info frame_info, View_ID view_id){
   ProfileScope(app, "default render caller");
   View_ID active_view = get_active_view(app, Access_Always);
   b32 is_active_view = (active_view == view_id);
   
   Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view);
   Rect_f32 prev_clip = draw_set_clip(app, region);
   
   Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
   Face_ID face_id = get_face_id(app, buffer);
   Face_Metrics face_metrics = get_face_metrics(app, face_id);
   f32 line_height = face_metrics.line_height;
   f32 digit_advance = face_metrics.decimal_digit_advance;
   
   // NOTE(allen): file bar
   b64 showing_file_bar = false;
   if (view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar){
      Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
      //draw_file_bar(app, view_id, buffer, face_id, pair.min);
      luis_draw_file_bar(app, view_id, buffer, face_id, pair.min);
      region = pair.max;
   }
   
   Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);
   
   Buffer_Point_Delta_Result delta = delta_apply(app, view_id,
                                                 frame_info.animation_dt, scroll);
   if (!block_match_struct(&scroll.position, &delta.point)){
      block_copy_struct(&scroll.position, &delta.point);
      view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
   }
   if (delta.still_animating){
      animate_in_n_milliseconds(app, 0);
   }
   
   // NOTE(allen): query bars
   region = default_draw_query_bars(app, region, view_id, face_id);
   
   // NOTE(allen): FPS hud
   if (show_fps_hud){
      Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
      draw_fps_hud(app, frame_info, face_id, pair.max);
      region = pair.min;
      animate_in_n_milliseconds(app, 1000);
   }
   
   // NOTE(allen): layout line numbers
   b32 show_line_number_margins = def_get_config_b32(vars_save_string_lit("show_line_number_margins"));
   Rect_f32 line_number_rect = {};
   if (show_line_number_margins){
      Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
      line_number_rect = pair.min;
      region = pair.max;
   }
   
   // NOTE(allen): begin buffer render
   Buffer_Point buffer_point = scroll.position;
   Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
   
   // NOTE(allen): draw line numbers
   if (show_line_number_margins){
      draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
   }
   
   // NOTE(allen): draw the buffer
   luis_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);
   
   text_layout_free(app, text_layout_id);
   draw_set_clip(app, prev_clip);
}


function void
luis_view_change_buffer(Application_Links *app, View_ID view_id,
                        Buffer_ID old_buffer_id, Buffer_ID new_buffer_id){
   Managed_Scope scope = view_get_managed_scope(app, view_id);
   Buffer_ID *prev_buffer_id = scope_attachment(app, scope, view_previous_buffer, Buffer_ID);
	if (prev_buffer_id != 0){
		*prev_buffer_id = old_buffer_id;
	}
   
   i32 *tab_group_index = scope_attachment(app, scope, view_tab_group_index, i32);
   if(tab_group_index)
   {
      if(old_buffer_id == 0) //view creation, set it's default tab group
      {
         //since panels always open by splitting, we should always get a child view here...
         if(MAKE_NEW_BUFFER_TAB_GROUP_ON_VIEW_CREATION)
         {
            MAKE_NEW_BUFFER_TAB_GROUP_ON_VIEW_CREATION = false;
            view_new_tab_group(app, view_id);
         }
         else
         {
            View_ID prev_view = luis_get_other_child_view(app, view_id);
            if(prev_view)
            {
               Managed_Scope prev_active_view_scope = view_get_managed_scope(app, prev_view);
               i32 *prev_tab_group_index = scope_attachment(app, prev_active_view_scope, view_tab_group_index, i32);
               if(prev_tab_group_index)
                  *tab_group_index = *prev_tab_group_index;
            }   
         }
      }
      if(*tab_group_index < 0 || *tab_group_index >= BUFFER_TAB_GROUP_COUNT)
         *tab_group_index = 1;
      
      //i32 *current_tab = scope_attachment(app, scope, view_current_tab, i32);
      //if(current_tab)
      {
         Buffer_Tab_Group *group = BUFFER_TAB_GROUPS + *tab_group_index;
         b32 tab_with_id_already_present = find_tab_with_buffer_id(group, new_buffer_id, &group->current_tab);
         
         if(luis_view_has_flags(app, view_id, VIEW_ADD_NEW_BUFFER_AS_NEW_TAB)) 
         {
            luis_view_clear_flags(app, view_id, VIEW_ADD_NEW_BUFFER_AS_NEW_TAB);
            //make new one if no tab present and have space
            if(!tab_with_id_already_present && group->tab_count < countof(group->tabs))
            {
               group->tab_count += 1;
               for(i32 tab_index = group->tab_count - 1; tab_index > (group->current_tab + 1); tab_index -= 1)
                  group->tabs[tab_index] = group->tabs[tab_index - 1];
               group->current_tab += 1;
               //group->current_tab = group->tab_count++; //add tab to the very end of array 
            }
               
         }
         group->tabs[group->current_tab] = new_buffer_id;
      }
   }
}

internal void
luis_set_hooks(Application_Links *app){
   set_custom_hook(app, HookID_BufferViewerUpdate, default_view_adjust);
   
   set_custom_hook(app, HookID_ViewEventHandler, luis_view_input_handler);
   set_custom_hook(app, HookID_Tick, default_tick);
   set_custom_hook(app, HookID_RenderCaller, luis_render_caller);
   set_custom_hook(app, HookID_WholeScreenRenderCaller, default_whole_screen_render_caller);
   
   set_custom_hook(app, HookID_DeltaRule, fixed_time_cubic_delta);
   set_custom_hook_memory_size(app, HookID_DeltaRule,
                               delta_ctx_size(fixed_time_cubic_delta_memory_size));
   
   set_custom_hook(app, HookID_BufferNameResolver, default_buffer_name_resolution);
   
   set_custom_hook(app, HookID_BeginBuffer, default_begin_buffer);
   set_custom_hook(app, HookID_EndBuffer, end_buffer_close_jump_list);
   set_custom_hook(app, HookID_NewFile, default_new_file);
   set_custom_hook(app, HookID_SaveFile, default_file_save);
   set_custom_hook(app, HookID_BufferEditRange, default_buffer_edit_range);
   set_custom_hook(app, HookID_BufferRegion, default_buffer_region);
   set_custom_hook(app, HookID_ViewChangeBuffer, luis_view_change_buffer);
   
   set_custom_hook(app, HookID_Layout, layout_unwrapped);
   //set_custom_hook(app, HookID_Layout, layout_wrap_anywhere);
   //set_custom_hook(app, HookID_Layout, layout_wrap_whitespace);
   //set_custom_hook(app, HookID_Layout, layout_virt_indent_unwrapped);
   //set_custom_hook(app, HookID_Layout, layout_unwrapped_small_blank_lines);
}