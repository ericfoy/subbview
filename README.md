# Views Table with Subviews

**Views Table with Subviews** provides a Views style plugin for Backdrop CMS that renders a table whose rows can expand to reveal an embedded subview beneath each parent row.

The module builds on **Views Enhanced Table**, so it includes the enhanced table controls for column ordering, merged columns, widths, borders, click-sorting, and related table display options, while adding row-by-row embedded subview functionality.

## Features

All shared **Enhanced Table** display controls are available, along with Subview-specific functionality such as:

- **Embedded subview beneath each parent row**
- **Expand / collapse controls** for each row
- Optional **initially collapsed** display
- Optional **expand-exclusive (accordion) behavior**
- Optional **indentation** of the embedded subview row
- **Per-row expand button column** selection
- **Optional subview embedding** (the display can behave as a plain table when embedding is disabled)
- **Nested subviews** support  
  A subview may itself be another “Table with Subviews” display
- **Recursion protection** to help prevent circular self-embedding configurations from rendering indefinitely

Because the module depends on **Views Enhanced Table**, Subview displays also benefit from:

- **Presentation-only column ordering**
- **Merged columns**
- **Inline titles for merged fields**
- **Per-column width**
- Optional borders with thickness and color controls:
  - **Horizontal row borders**
  - **Vertical column borders**
  - **Outer table frame**

## Requirements

- Backdrop CMS 1.x
- Views (core module)
- Views Enhanced Table

## Installation

The easiest way to install the module on an existing Backdrop CMS site is to navigate to `admin/modules/install` and search for "views table with subviews". Otherwise,

- Install this module using the official Backdrop CMS instructions at  
  https://docs.backdropcms.org/documentation/extend-with-modules
- Enable the module at:  
  **Administration → Functionality → List modules**

Because this module depends on **Views Enhanced Table**, be sure that module is installed and enabled as well.

## Usage

1. Edit a View.
2. In the display’s **Format** section, choose:
   - **Show:** Table
   - **Style plugin:** **Table with Subviews**
3. Click **Settings** next to the style plugin.
4. Configure the shared table options as desired:
   - Columns
   - Styling
   - Mechanics
   - Admin
5. In **Subview settings**, enable:
   - **Embed a Subview for each row**
6. Select:
   - **Subview target**
   - **Argument source field**
7. Optionally configure:
   - **Per-row expand button column**
   - **Indent Subview**
   - **Start collapsed on initial load**
   - **Expand-exclusive (accordion style)**
   - **Allow embedding the same view**
8. Save the View and test the row expansion behavior.

## How it works

For each parent row in the main table, the module can render a secondary row immediately beneath it containing the selected subview.

The subview receives its contextual argument from the field selected in **Argument source field**.

This makes it possible to build master/detail displays such as:

- customers → orders
- projects → tasks
- invoices → line items
- assemblies → components

## Nested subviews

A Subview may itself use the **Table with Subviews** style. This allows multi-level nested displays.

Use this carefully. Deep nesting can increase render cost, and circular embedding relationships should be avoided.

Examples to avoid:

- a display embedding itself directly
- View A embedding View B while View B embeds View A

The module includes recursion safeguards, but good View design is still recommended.

## Configuration storage

Table with Subviews does not create a separate module-level configuration object for normal display settings.

Its settings are stored per View display inside the View configuration, primarily in `display_options`, `style_plugin`, and `style_options`.

## Styling notes

Subview displays inherit the shared table feature set from **Views Enhanced Table**.

Child subviews may also inherit some styling from the parent table, depending on the rendered markup and CSS selectors involved. In many cases this can be overridden by styling applied to the subview itself.

## Disabling / uninstalling

This module provides a Views style plugin. Any View display using **Table with Subviews** depends on the module being enabled.

Before disabling or uninstalling the module permanently, switch affected View displays to another style plugin.

Because this module depends on **Views Enhanced Table**, those displays also depend on Enhanced Table remaining available.

## Notes

- The subview target should be designed to accept a contextual filter matching the selected **Argument source field**.
- Nested subviews are supported, but performance should be tested carefully on large result sets.
- Click sorting and table styling apply independently to each rendered table.

## Issues

- Please report bugs and feature requests in the project's issue queue.
- Include Backdrop core version, PHP version, and steps to reproduce.

## Current Maintainers

- [ericfoy](https://github.com/ericfoy)

## Credits

- This module was created for Backdrop by [ericfoy](https://github.com/ericfoy)
- Sponsored by [Perideo LLC](https://perideo.com)

## License


This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
