<?php

// Load EnhancedTable shared base (Phase 4 integration).
if (module_exists('enhancedtable')) {
  module_load_include('inc', 'enhancedtable', 'views/enhancedtable_base_style_table');
}

class views_plugin_style_subbview_table extends enhancedtable_base_style_table {

  protected $icon_closed = '▼'; // right arrow ⁎↓→■□○◌◊♦►▼●ꜜꜛ⸕⸎→↑↓↕↨∆˃˂˄˅ǂǁ
  protected $icon_open   = '■'; // down arrow

  public function option_definition() {
    $o = parent::option_definition();
    // NULL means "not explicitly set yet" so legacy configured displays can
    // infer enabled behavior while brand-new displays remain off by default.
    $o['embed_subview']     = array('default' => NULL);
    $o['subbview_view']     = array('default' => '');
    $o['subbview_display']  = array('default' => 'default');
    $o['arg_source_field']  = array('default' => '');
    $o['start_collapsed']   = array('default' => TRUE);
    $o['indent_px']         = array('default' => 32);
    $o['stripe_rows']       = array('default' => TRUE);  // legacy back-compat only
    $o['accordion']         = array('default' => FALSE);
    $o['allow_same_view'] = array('default' => TRUE); // optional toggle
    $o['row_toggle_column'] = array('default' => ''); // '' = first visible column (default)
    return $o;
  }

  public function options_form(&$form, &$form_state) {
    // Back-compat: older Subbview versions used a separate "stripe_rows" option.
    // Only map that legacy value when the standard row_class_special option
    // is truly absent, otherwise an intentional unchecked state gets forced
    // back on every time the form is opened.
    if (!array_key_exists('row_class_special', $this->options) && array_key_exists('stripe_rows', $this->options) && !empty($this->options['stripe_rows'])) {
      $this->options['row_class_special'] = 1;
    }

    // Build the EnhancedTable-style UI (cards, styling, mechanics, admin).
    parent::options_form($form, $form_state);

    // Ensure the standard striping widget is always present in Styling.
    // Some earlier configs relied on the legacy Subbview-only stripe_rows
    // option, so fall back to that when the core widget is absent.
    if (!isset($form['bt_styling']['row_class_special'])) {
      if (isset($form['row_class_special'])) {
        $form['bt_styling']['row_class_special'] = $form['row_class_special'];
        $form['bt_styling']['row_class_special']['#weight'] = 0;
        $form['bt_styling']['row_class_special']['#parents'] = array('style_options', 'row_class_special');
        unset($form['row_class_special']);
      }
      else {
        $default_row_class_special = array_key_exists('row_class_special', $this->options)
          ? !empty($this->options['row_class_special'])
          : !empty($this->options['stripe_rows']);

        $form['bt_styling']['row_class_special'] = array(
          '#type' => 'checkbox',
          '#title' => t('Add striping (odd/even), first/last row classes'),
          '#default_value' => $default_row_class_special,
          '#parents' => array('style_options', 'row_class_special'),
          '#weight' => 0,
        );
      }
    }

    if (empty($this->display) || empty($this->display->handler)) {
      return;
    }

    $selected = (!empty($this->options['subbview_view']) && !empty($this->options['subbview_display']))
      ? ($this->options['subbview_view'] . ':' . $this->options['subbview_display'])
      : '';

    // Note: Views merges option defaults into $this->options, so we cannot use
    // array_key_exists() here to detect legacy saved displays. NULL means the
    // checkbox has never been explicitly saved yet.
    $embed_subview_default = isset($this->options['embed_subview'])
      ? !empty($this->options['embed_subview'])
      : ($selected !== '');

    // ------------------------------------------------------------
    // Subview settings fieldset (inserted between Styling and Mechanics).
    // ------------------------------------------------------------

    $form['bt_subview'] = array(
      '#type'        => 'fieldset',
      '#title'       => t('Subview settings'),
      '#collapsible' => TRUE,
      '#collapsed'   => FALSE,
      '#weight'      => 35,
    );

    $form['bt_subview']['embed_subview'] = array(
      '#type' => 'checkbox',
      '#title' => t('Embed a Subview for each row'),
      '#default_value' => $embed_subview_default,
      '#parents' => array('style_options', 'embed_subview'),
      '#weight' => 0,
    );

    // Keep originals as hidden storage.
    $form['bt_subview']['subbview_view'] = array(
      '#type' => 'hidden',
      '#default_value' => isset($this->options['subbview_view']) ? $this->options['subbview_view'] : '',
      '#parents' => array('style_options', 'subbview_view'),
    );
    $form['bt_subview']['subbview_display'] = array(
      '#type' => 'hidden',
      '#default_value' => isset($this->options['subbview_display']) ? $this->options['subbview_display'] : 'default',
      '#parents' => array('style_options', 'subbview_display'),
    );

    $subview_states = array(
      'visible' => array(
        ':input[name="style_options[embed_subview]"]' => array('checked' => TRUE),
      ),
    );

    // ------------------------------------------------------------
    // Per-row toggle column (root columns only, so merged columns are valid).
    // ------------------------------------------------------------

    $handlers = $this->display->handler->get_handlers('field');
    $field_labels = $this->display->handler->get_field_labels();

    // Visible (non-excluded) fields.
    $visible_fields = array();
    foreach ((array) $handlers as $fid => $handler) {
      if (!empty($handler->options['exclude'])) {
        continue;
      }
      $visible_fields[] = $fid;
    }

    // Order fields using EnhancedTable weights if present.
    $ordered = $visible_fields;
    $weights = (!empty($this->options['field_order']) && is_array($this->options['field_order']))
      ? $this->options['field_order']
      : array();
    usort($ordered, function($a, $b) use ($weights) {
      $wa = isset($weights[$a]) ? (int) $weights[$a] : 0;
      $wb = isset($weights[$b]) ? (int) $weights[$b] : 0;
      if ($wa == $wb) {
        return strcmp($a, $b);
      }
      return ($wa < $wb) ? -1 : 1;
    });

    // Root columns only (skip merged children).
    $merge_left = (!empty($this->options['merge_left']) && is_array($this->options['merge_left']))
      ? $this->options['merge_left']
      : array();
    $root_cols = array();
    $root = NULL;
    foreach ($ordered as $fid) {
      $want_merge = !empty($merge_left[$fid]);
      if ($want_merge && $root) {
        continue;
      }
      $root = $fid;
      $root_cols[] = $fid;
    }

    $row_toggle_options = array('' => t('- First visible column (default) -'));
    foreach ($root_cols as $fid) {
      $label = isset($field_labels[$fid]) ? $field_labels[$fid] : $fid;
      $row_toggle_options[$fid] = $label . ' (' . $fid . ')';
    }

    $form['bt_subview']['row_toggle_column'] = array(
      '#type' => 'select',
      '#title' => t('Per-row expand button column'),
      '#description' => t('Choose which column contains the per-row expand/collapse button. The header “expand all” button always stays in the first column.'),
      '#options' => $row_toggle_options,
      '#default_value' => isset($this->options['row_toggle_column']) ? $this->options['row_toggle_column'] : '',
      '#parents' => array('style_options', 'row_toggle_column'),
      '#states' => $subview_states,
      '#weight' => 30,
    );

    // ------------------------------------------------------------
    // Subview target selection.
    // ------------------------------------------------------------

    // Build "View: Display" dropdown (optgroups per view).
    $targets = array('' => t('- Select -'));
    foreach (views_get_all_views() as $v) {
      if (!empty($v->disabled)) {
        continue;
      }
      $group = $v->human_name . ' (' . $v->name . ')';
      $opts = array();
      foreach ($v->display as $did => $disp) {
        $title = !empty($disp->display_title) ? $disp->display_title : $did;
        $hint = '';
        if (!empty($disp->display_options['path'])) {
          $hint = ' — /' . ltrim($disp->display_options['path'], '/');
        }
        elseif ($disp->display_plugin === 'block' && !empty($disp->id)) {
          $hint = ' — block:' . $disp->id;
        }
        $opts[$v->name . ':' . $did] = $v->name . ': ' . $title . ' [' . $did . ']' . $hint;
      }
      if ($opts) {
        $targets[$group] = $opts;
      }
    }

    $form['bt_subview']['subbview_target'] = array(
      '#type' => 'select',
      '#title' => t('Subview target'),
      '#description' => t('This is the subview which will be passed the below specified argument, and will display beneath each row.'),
      '#options' => $targets,
      '#default_value' => $selected,
      '#required' => FALSE,
      '#states' => $subview_states,
      '#weight' => 10,
      '#parents' => array('style_options', 'subbview_target'),
    );

    // ------------------------------------------------------------
    // Argument source and behavior options.
    // ------------------------------------------------------------

    $field_options = array('' => t('- Select -'));
    foreach ((array) $handlers as $id => $handler) {
      $field_options[$id] = $handler->ui_name();
    }

    $form['bt_subview']['arg_source_field'] = array(
      '#type' => 'select',
      '#title' => t('Argument source field'),
      '#description' => t('The value of this field (for each row) will be passed to the subview specified above.'),
      '#options' => $field_options,
      '#default_value' => isset($this->options['arg_source_field']) ? $this->options['arg_source_field'] : '',
      '#parents' => array('style_options', 'arg_source_field'),
      '#states' => $subview_states,
      '#weight' => 20,
    );

    $form['bt_subview']['start_collapsed'] = array(
      '#type' => 'checkbox',
      '#title' => t('Start collapsed on initial load'),
      '#default_value' => !empty($this->options['start_collapsed']),
      '#parents' => array('style_options', 'start_collapsed'),
      '#states' => $subview_states,
      '#weight' => 50,
    );

    $form['bt_subview']['accordion'] = array(
      '#type' => 'checkbox',
      '#title' => t('Expand-exclusive (accordion style)'),
      '#description' => t('Only one subview stays open at a time. Others automatically close.'),
      '#default_value' => !empty($this->options['accordion']),
      '#parents' => array('style_options', 'accordion'),
      '#states' => $subview_states,
      '#weight' => 60,
    );

    $form['bt_subview']['allow_same_view'] = array(
      '#type' => 'checkbox',
      '#title' => t('Allow embedding the same view'),
      '#description' => t('If disabled, Subbview will never embed a subview that uses the same parent view name (useful as an extra safeguard beyond recursion detection).'),
      '#default_value' => !empty($this->options['allow_same_view']),
      '#parents' => array('style_options', 'allow_same_view'),
      '#states' => $subview_states,
      '#weight' => 70,
    );

    $form['bt_subview']['indent_px'] = array(
      '#type' => 'textfield',
      '#title' => t('Indent Subview'),
      '#description' => t('CSS length. Examples: 32px, 1.25em, 10%, 0. If you enter a plain number, px will be assumed.'),
      '#default_value' => isset($this->options['indent_px']) ? $this->options['indent_px'] : '32',
      '#size' => 10,
      '#element_validate' => array('subbview_element_validate_css_length'),
      '#parents' => array('style_options', 'indent_px'),
      '#states' => $subview_states,
      '#weight' => 40,
    );

    if (function_exists('subbview_build_subview_help_form')) {
      $form['bt_subview_help'] = subbview_build_subview_help_form();
      $form['bt_subview_help']['#weight'] = 1;
    }
  }


  public function render() {
    // Cycle guard.
    // Use a fixed static key so recursion protection remains effective even
    // if this class is later refactored/inherited.
    $stack = &backdrop_static('subbview_render_stack', array());
    $current_key = $this->view->name . ':' . $this->view->current_display;
    if (!empty($stack[$current_key])) {
      return parent::render();
    }
    $stack[$current_key] = TRUE;

    $out = NULL; // final output
    try {
      do {
        // Apply EnhancedTable-style CSS rules to this view's *root* table only.
        // This is scoped so it will not affect embedded subviews.
        if (module_exists('enhancedtable')) {
          module_load_include('inc', 'enhancedtable', 'includes/enhancedtable.shared');
          if (function_exists('enhancedtable_get_root_table_selector') && function_exists('enhancedtable_build_table_css')) {
            $table_selector = enhancedtable_get_root_table_selector($this->view);
            $css = enhancedtable_build_table_css($this->options, $table_selector);
            if ($css !== '') {
              backdrop_add_css($css, array('type' => 'inline'));
            }
          }
        }

        // Decide whether to embed subviews for each row.
        // Back-compat: older saved displays will not have embed_subview yet.
        // In that case, infer enabled when the legacy subview target + argument
        // source are already configured.
        $embed_enabled = isset($this->options['embed_subview'])
          ? !empty($this->options['embed_subview'])
          : (!empty($this->options['subbview_view']) && !empty($this->options['arg_source_field']));

        // An explicitly enabled subview still requires a valid target + arg source.
        if ($embed_enabled && (empty($this->options['subbview_view']) || empty($this->options['arg_source_field']))) {
          $embed_enabled = FALSE;
        }

        if ($embed_enabled) {
          backdrop_add_library('subbview', 'subbview');
        }

        $fields = &$this->view->field;
        $result = $this->view->result;
        if (empty($result)) {
          $out = parent::render();
          break;
        }

        // Render fields using Views core (and EnhancedTable base) so that
        // inline-titles and other shared behaviors apply.
        $this->render_fields($result);

        // Determine visible field order.
        $visible_fields = array();
        foreach ((array) $fields as $fid => $handler) {
          if (empty($handler) || !is_object($handler)) {
            continue;
          }
          if (!empty($handler->options['exclude'])) {
            continue;
          }
          $visible_fields[] = $fid;
        }

        // Determine column grouping from Views "columns" mapping.
        // Multiple fields may map to the same root (merge-left).
        $colmap = (!empty($this->options['columns']) && is_array($this->options['columns']))
          ? $this->options['columns']
          : array();

        $ordered_columns = array();   // list of root column field IDs
        $column_members = array();    // root => [field ids...]
        foreach ($visible_fields as $fid) {
          $col = isset($colmap[$fid]) ? $colmap[$fid] : $fid;

          // Sanity: root column must be an existing, visible field.
          if (empty($fields[$col]) || !is_object($fields[$col]) || !empty($fields[$col]->options['exclude'])) {
            $col = $fid;
          }

          if (!isset($column_members[$col])) {
            $ordered_columns[] = $col;
            $column_members[$col] = array();
          }
          $column_members[$col][] = $fid;
        }

        if (empty($ordered_columns)) {
          // Fallback to at least one column so the table theme doesn't error.
          $ordered_columns = array('');
          $column_members[''] = array();
        }

        // Build header cells (one per root column).
        $header = array();

        $active = !empty($this->active) ? $this->active : '';
        $order  = !empty($this->order)  ? $this->order  : 'asc';

        $query = tablesort_get_query_parameters();
        if (isset($this->view->exposed_raw_input)) {
          $query = $query + (is_array($this->view->exposed_raw_input)
            ? $this->view->exposed_raw_input
            : array());
        }

        foreach ($ordered_columns as $col) {
          if ($col === '' || empty($fields[$col]) || !is_object($fields[$col])) {
            $header[] = '';
            continue;
          }

          $label = check_plain((string) $fields[$col]->label());
          $sortable_allowed = !empty($this->options['info'][$col]['sortable']) && $fields[$col]->click_sortable();

          $cell = array();
          if ($sortable_allowed && $label !== '') {
            $initial = !empty($this->options['info'][$col]['default_sort_order'])
              ? $this->options['info'][$col]['default_sort_order'] : 'asc';
            if ($active == $col) {
              $initial = ($order == 'asc') ? 'desc' : 'asc';
            }
            $title = t('sort by @s', array('@s' => $label));
            $link_label = $label;
            if ($active == $col) {
              $link_label .= theme('tablesort_indicator', array('style' => $initial));
            }
            $link_query = $query + array('order' => $col, 'sort' => $initial);
            $cell['data'] = array('#markup' => l($link_label, $_GET['q'], array(
              'html' => TRUE,
              'attributes' => array('title' => $title),
              'query' => $link_query,
            )));
          }
          else {
            $cell['data'] = array('#markup' => $label);
          }

          // Column width (EnhancedTable shared option), with fallback to Subbview legacy storage.
          $w = '';
          if (!empty($this->options['column_widths']) && is_array($this->options['column_widths']) && isset($this->options['column_widths'][$col])) {
            $w = trim((string) $this->options['column_widths'][$col]);
          }
          elseif (isset($this->options['info'][$col]['width'])) {
            $w = trim((string) $this->options['info'][$col]['width']);
          }
          if ($w !== '') {
            $cell['style'] = 'width:' . check_plain($w) . ';';
          }

          $classes = '';
          if ($fields[$col]->options['element_default_classes']) {
            $classes = 'views-field views-field-' . backdrop_clean_css_identifier($col);
          }
          if ($c = $fields[$col]->element_label_classes(0)) {
            $classes = $classes ? $classes . ' ' . $c : $c;
          }
          if (!empty($this->options['info'][$col]['align'])) {
            $classes .= ' ' . backdrop_clean_css_identifier($this->options['info'][$col]['align']);
          }
          if ($classes) {
            $cell['class'] = preg_split('/\s+/', trim($classes));
          }

          $header[] = $cell;
        }

        if (!array_filter($header)) {
          $header[] = '';
        }
        $num_cols = max(1, count($header));

        // Determine which *rendered column* gets the per-row expand/collapse button.
        // Header "expand all" stays in the first header cell (handled later).
        $row_toggle_field = isset($this->options['row_toggle_column']) ? (string) $this->options['row_toggle_column'] : '';
        if ($row_toggle_field !== '' && isset($colmap[$row_toggle_field])) {
          $row_toggle_field = (string) $colmap[$row_toggle_field];
        }
        $row_toggle_index = 0; // default: first visible column
        if ($row_toggle_field !== '') {
          $found = array_search($row_toggle_field, $ordered_columns, TRUE);
          if ($found !== FALSE) {
            $row_toggle_index = (int) $found;
          }
        }

        // Table-wide controls and state.
        $arg_field = $this->options['arg_source_field'];
        $child_v   = $this->options['subbview_view'];
        $child_d   = $this->options['subbview_display'];
        $collapsed = !empty($this->options['start_collapsed']);

        $indent = isset($this->options['indent_px']) ? trim((string) $this->options['indent_px']) : '0';
        if ($indent === '') {
          $indent = '0';
        }
        // Back-compat: old configs stored as numbers -> assume px.
        if (preg_match('/^-?\d+(\.\d+)?$/', $indent)) {
          $indent .= 'px';
        }
        // Extra safety (in case config was edited outside the form).
        if ($indent !== '0' && !preg_match('/^-?\d+(\.\d+)?(px|em|rem|%|vw|vh|vmin|vmax|ch|ex|cm|mm|in|pt|pc)$/i', $indent)) {
          $indent = '0';
        }

        if ($embed_enabled) {
          // Inject master toggle in first header cell.
          reset($header);
          $first_hkey = key($header);
          $btn = '<button type="button" class="sv-toggle sv-toggle-all" aria-expanded="' .
                ($collapsed ? 'false' : 'true') . '">' .
                ($collapsed ? $this->icon_closed : $this->icon_open) . '</button>';
          $orig = '';
          $prev_classes = array();
          $prev_style = '';
          if (isset($header[$first_hkey]) && is_array($header[$first_hkey])) {
            $d = isset($header[$first_hkey]['data']) ? $header[$first_hkey]['data'] : '';
            $orig = is_array($d) && isset($d['#markup']) ? (string) $d['#markup'] : (string) $d;
            if (!empty($header[$first_hkey]['class'])) $prev_classes = (array) $header[$first_hkey]['class'];
            if (!empty($header[$first_hkey]['style'])) $prev_style = $header[$first_hkey]['style'];
          }
          else {
            $orig = (string) $header[$first_hkey];
          }
          $header[$first_hkey] = array(
            'data'  => array('#markup' => $btn . ' ' . $orig),
            'class' => array_merge(array('sv-toggle-col'), $prev_classes),
          );
          if ($prev_style !== '') {
            $header[$first_hkey]['style'] = $prev_style;
          }
        }

        // Build rows.
        $rows = array();

        // Row striping (odd/even). Prefer the standard table option.
        $do_striping = TRUE;
        if (array_key_exists('row_class_special', $this->options)) {
          $do_striping = !empty($this->options['row_class_special']);
        }
        elseif (array_key_exists('stripe_rows', $this->options)) {
          $do_striping = !empty($this->options['stripe_rows']);
        }

        foreach ($result as $i => $row) {
          // Keep row context sane for handlers that look at it.
          $this->view->row_index = $i;

          // Build parent row cells in the SAME order as header (root columns).
          $row_cells = array();

          foreach ($ordered_columns as $col) {
            if ($col === '' || empty($fields[$col]) || !is_object($fields[$col])) {
              $row_cells[] = array('data' => '');
              continue;
            }

            $separator = isset($this->options['info'][$col]['separator'])
              ? (string) $this->options['info'][$col]['separator']
              : ' ';

            $parts = array();
            if (!empty($column_members[$col]) && is_array($column_members[$col])) {
              foreach ($column_members[$col] as $fid) {
                $val = isset($this->rendered_fields[$i][$fid]) ? $this->rendered_fields[$i][$fid] : '';
                if ($this->enhancedtable_is_effectively_empty($val)) {
                  continue;
                }
                $parts[] = $val;
              }
            }

            // If everything is empty, keep the first field's output (so "0" etc can show).
            if (empty($parts) && !empty($column_members[$col][0])) {
              $fid0 = $column_members[$col][0];
              $markup = isset($this->rendered_fields[$i][$fid0]) ? $this->rendered_fields[$i][$fid0] : '';
            }
            else {
              $markup = implode($separator, $parts);
            }

            // Recreate core table style cell classes (based on the *root column* field).
            $classes = array();
            if (!empty($fields[$col]->options['element_default_classes'])) {
              $classes[] = 'views-field';
              $classes[] = 'views-field-' . backdrop_clean_css_identifier($col);
            }

            // Includes things like "views-field-editablefield".
            if (method_exists($fields[$col], 'element_classes')) {
              $c = trim((string) $fields[$col]->element_classes($i));
              if ($c !== '') {
                $classes = array_merge($classes, preg_split('/\s+/', $c));
              }
            }

            if (!empty($this->options['info'][$col]['align'])) {
              $classes[] = backdrop_clean_css_identifier($this->options['info'][$col]['align']);
            }

            $cell = array('data' => $markup);
            if ($classes) {
              $cell['class'] = $classes;
            }

            $row_cells[] = $cell;
          }

          // Inject per-row toggle in chosen column (defaults to first visible cell).
          if ($embed_enabled && !empty($row_cells)) {
            $rk = isset($row_cells[$row_toggle_index]) ? $row_toggle_index : 0;
            $existing = isset($row_cells[$rk]['data']) ? $row_cells[$rk]['data'] : '';
            $row_cells[$rk]['data'] =
              '<button type="button" class="sv-toggle" aria-expanded="' . ($collapsed ? 'false' : 'true') . '">' .
              ($collapsed ? $this->icon_closed : $this->icon_open) . '</button> ' .
              $existing;

            // Optional: makes the toggle cell targetable.
            if (empty($row_cells[$rk]['class'])) {
              $row_cells[$rk]['class'] = array();
            }
            // Differentiate from the header's sv-toggle-col (which stays in column 1).
            $row_cells[$rk]['class'][] = 'sv-row-toggle-col';
          }

          $rows[] = array(
            'data'        => $row_cells,
            'class'       => array('sv-parent'),
            'no_striping' => !$do_striping,
          );

          if ($embed_enabled) {
            // --- Subview row ---
            $arg = isset($fields[$arg_field]) ? $fields[$arg_field]->get_value($row) : NULL;
            $child_key = $child_v . ':' . $child_d;
            $can_embed = ($child_v && $child_d !== '' && $arg !== NULL && empty($stack[$child_key]));
            if (isset($this->options['allow_same_view']) && !$this->options['allow_same_view'] && $child_v === $this->view->name) {
              $can_embed = FALSE;
            }
            $child_markup = '';
            if ($can_embed) {
              // Preserve parent view context; views_embed_view() can change it.
              $prev_current_view = function_exists('views_get_current_view') ? views_get_current_view() : NULL;
              $prev_global_view  = isset($GLOBALS['view']) ? $GLOBALS['view'] : NULL;
              $prev_row_index    = isset($this->view->row_index) ? $this->view->row_index : NULL;

              $child_markup = views_embed_view($child_v, $child_d, $arg);

              if (function_exists('views_set_current_view')) {
                views_set_current_view($prev_current_view);
              }
              if ($prev_row_index !== NULL) {
                $this->view->row_index = $prev_row_index;
              }
              if ($prev_global_view !== NULL) {
                $GLOBALS['view'] = $prev_global_view;
              }
            }

            $sub_html = '<div class="sv-subwrap" style="padding-left:' . check_plain($indent) . '">' .
                        '<div class="sv-subbview" data-arg="' . check_plain($arg) . '">' . $child_markup . '</div>' .
                        '</div>';

            $rows[] = array(
              'data'        => array(array('data' => $sub_html, 'colspan' => $num_cols, 'class' => array('sv-subcell'))),
              'class'       => array('sv-subrow', $collapsed ? 'is-collapsed' : 'is-open'),
              'no_striping' => TRUE,
            );
          }
        }

        // Clean up.
        $this->view->row_index = NULL;

        // Emit <colgroup> so widths apply and JS can clone to child tables.
        // For merged columns, only emit one col per *root column*.
        $colgroup_cols = array();
        foreach ($ordered_columns as $col) {
          if ($col === '' || empty($fields[$col]) || !is_object($fields[$col])) {
            $colgroup_cols[] = array();
            continue;
          }
          $coldef = array();
          $w = '';
          if (!empty($this->options['column_widths']) && is_array($this->options['column_widths']) && isset($this->options['column_widths'][$col])) {
            $w = trim((string) $this->options['column_widths'][$col]);
          }
          elseif (isset($this->options['info'][$col]['width'])) {
            $w = trim((string) $this->options['info'][$col]['width']);
          }
          if ($w !== '') {
            $coldef['style'] = 'width:' . check_plain($w) . ';';
          }
          $colgroup_cols[] = $coldef;
        }

        $build = array(
          '#theme' => 'table',
          '#header' => $header,
          '#rows' => $rows,
          '#attributes' => array(
            'class' => array('views-table','views-view-table','sv-table'),
            'data-sv' => $this->view->name . ':' . $this->view->current_display,
            'data-icon-open' => $this->icon_open,
            'data-icon-closed' => $this->icon_closed,
            'data-accordion' => ($embed_enabled && !empty($this->options['accordion'])) ? '1' : '0',
          ),
          '#colgroups' => array(array('cols' => $colgroup_cols)),
        );

        $out = backdrop_render($build);
      } while (0);
    }
    finally {
      // Always unwind guards.
      unset($stack[$current_key]);
    }

    // Safety: fall back if something skipped setting $out.
    return ($out !== NULL) ? $out : parent::render();
  }


  public function options_submit(&$form, &$form_state) {
    parent::options_submit($form, $form_state);

    $so = &$form_state['values']['style_options'];

    if (!empty($so['subbview_target'])) {
      list($v, $d) = explode(':', $so['subbview_target'], 2);
      $so['subbview_view'] = $v;
      $so['subbview_display'] = $d;
    }

    // Keep the legacy stripe_rows value synchronized for backwards compatibility.
    if (array_key_exists('row_class_special', $so)) {
      $so['stripe_rows'] = !empty($so['row_class_special']) ? 1 : 0;
    }

    // A disabled subview should behave as a plain table without requiring the
    // user to discard previously selected target/field values.
    if (empty($so['embed_subview'])) {
      $so['embed_subview'] = 0;
    }
  }
}

function subbview_element_validate_css_length($element, &$form_state, $form) {
  $value = trim((string) $element['#value']);
  if ($value === '') {
    $value = '0';
  }

  // Block obvious CSS injection.
  if (preg_match('/[;{}]/', $value)) {
    form_error($element, t('Invalid indent value.'));
    return;
  }

  // Bare number is allowed (px assumed later).
  if (preg_match('/^-?\d+(\.\d+)?$/', $value)) {
    form_set_value($element, $value, $form_state);
    return;
  }

  // Number + unit.
  $units = '(px|em|rem|%|vw|vh|vmin|vmax|ch|ex|cm|mm|in|pt|pc)';
  if ($value === '0' || preg_match('/^-?\d+(\.\d+)?' . $units . '$/i', $value)) {
    form_set_value($element, $value, $form_state);
    return;
  }

  form_error($element, t('Indent must be a CSS length (e.g. 32px, 1.25em, 10%) or 0.'));
}
