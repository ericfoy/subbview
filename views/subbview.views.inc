<?php

function subbview_views_plugins() {
//  $views_dir = backdrop_get_path('module', 'subbview') . '/views';
  return [
    'style' => [
      'subbview_table' => [
        'title' => t('Table with subviews'),
        'help'  => t('Table with collapsible subbview rows.'),
        'handler' => 'views_plugin_style_subbview_table',
//        'file' => 'views_plugin_style_subbview_table.inc',
//        'path' => $views_dir,
        'parent' => 'table',
        'type' => 'normal',
        'uses fields' => TRUE,
        'uses options' => TRUE,
        'uses row plugin' => FALSE,
      ],
    ],
  ];
}
