(function ($, Backdrop) {
  "use strict";

  function icons($t){
    return {
      open:  $t.attr('data-icon-open')   || '▼',
      closed:$t.attr('data-icon-closed') || '►'
    };
  }

  function setAll($t, open) {
    var ic = icons($t);
    var $subs = $t.find('tr.sv-subrow');
    var $btns = $t.find('button.sv-toggle').not('.sv-toggle-all');
    $subs.toggleClass('is-open', open).toggleClass('is-collapsed', !open);
    $btns.attr('aria-expanded', open ? 'true' : 'false')
         .text(open ? ic.open : ic.closed);
  }

  Backdrop.behaviors.subbviewMain = {
    attach: function (context) {
      var $tables = $('.sv-table', context);

      $tables.off('.svAll .svRow'); // clear old handlers

      // Header "toggle all".
      $tables.on('click.svAll', 'button.sv-toggle-all', function (e) {
        e.preventDefault();
        e.stopPropagation();

        var $btn = $(this);
        var $own = $btn.closest('table.sv-table');   // innermost table for this button
        var $t   = $(e.delegateTarget);              // table this handler is bound on
        if (!$own.is($t)) return;                    // ignore events from nested tables

        var ic   = icons($t);
        var open = $btn.attr('aria-expanded') !== 'true';
        setAll($t, open);
        $btn.attr('aria-expanded', open ? 'true' : 'false')
            .text(open ? ic.open : ic.closed);
      });

      // Row toggle (with optional accordion).
      $tables.on('click.svRow', 'button.sv-toggle:not(.sv-toggle-all)', function (e) {
        e.preventDefault();
        e.stopPropagation();

        var $btn = $(this);
        var $own = $btn.closest('table.sv-table');
        var $t   = $(e.delegateTarget);
        if (!$own.is($t)) return;

        var ic   = icons($t);
        var $row = $btn.closest('tr');
        var $sub = $row.next('tr.sv-subrow');
        var open = !$sub.hasClass('is-open');

        if (open && $t.attr('data-accordion') === '1') {
          var $others = $t.find('tr.sv-subrow.is-open').not($sub);
          $others.removeClass('is-open').addClass('is-collapsed');
          $others.prev('tr.sv-parent').find('button.sv-toggle')
            .attr('aria-expanded','false').text(ic.closed);
        }

        $sub.toggleClass('is-open', open).toggleClass('is-collapsed', !open);
        $btn.attr('aria-expanded', open ? 'true' : 'false')
            .text(open ? ic.open : ic.closed);
      });
    }
  };

/************** Editable fields hack -see note below ******************************/

  // Serialize isolation for nested Views/editablefields inside subbview rows.
  // Ensures that when a parent-row editablefield submits, subview inputs are excluded,
  // and when a subview editablefield submits, parent inputs are excluded.
  // --- Editablefields / nested Views isolation (cascades across multiple nesting levels) ---

  function disableOutOfScopeInputs($serializedForm, $scopeView) {
    if (!$serializedForm.length || !$scopeView.length) return $();

    var scopeEl = $scopeView.get(0);

    // Allowed: inputs inside this view, but NOT inside any nested sub-view.
    var $allowed = $scopeView.find(':input').filter(function () {
      var $v = jQuery(this).closest('.view');
      return $v.length && $v.get(0) === scopeEl;
    });

    // Disable everything else that would otherwise get serialized with this AJAX submit.
    var $toDisable = $serializedForm.find(':input').not($allowed).filter(function () {
      return !this.disabled;
    });

    $toDisable.addClass('sv-scope-disabled').prop('disabled', true);
    return $toDisable;
  }

  function reenableInputs($inputs) {
    if (!$inputs || !$inputs.length) return;
    $inputs.prop('disabled', false).removeClass('sv-scope-disabled');
  }

  Backdrop.behaviors.subbviewEditablefieldsIsolation = {
    attach: function (context) {

      // Patch the ajax prototype ONCE so it applies to AJAX objects created later too.
      if (!Backdrop.ajax || !Backdrop.ajax.prototype) return;
      if (Backdrop.ajax.prototype._svEfIsoPatched) return;
      Backdrop.ajax.prototype._svEfIsoPatched = true;

      var proto = Backdrop.ajax.prototype;

      var origBeforeSerialize = proto.beforeSerialize;
      var origSuccess         = proto.success;
      var origError           = proto.error;

      proto.beforeSerialize = function (element, options) {
        // In Backdrop/Drupal, `element` can be the FORM, not the triggering submit.
        // The true trigger is usually `this.element`.
        var trigger = this.element || element;
        var $trigger = jQuery(trigger);

        // Only scope-fix editablefields submits.
        var isEditablefieldsSubmit =
          (trigger && trigger.name && String(trigger.name).indexOf('submit-field_') === 0) ||
          (trigger && trigger.id   && String(trigger.id).indexOf('-actions-submit') !== -1) ||
          $trigger.closest('.editablefield-item').length;

        if (isEditablefieldsSubmit) {
          // The actual form that will be serialized.
          var $serializedForm = $trigger.closest('form');
          if (!$serializedForm.length) {
            if (this.form) $serializedForm = jQuery(this.form);
            else if (this.$form && this.$form.length) $serializedForm = this.$form;
            else $serializedForm = jQuery(element).closest('form');
          }

          // Scope view is the innermost .view that contains the triggering element.
          var $scopeView = $trigger.closest('.view');
          if (!$scopeView.length) {
            $scopeView = jQuery(element).closest('.view');
          }

          if ($serializedForm.length && $scopeView.length) {
            this.svEfIsoDisabled = disableOutOfScopeInputs($serializedForm, $scopeView);
          }
        }

        if (typeof origBeforeSerialize === 'function') {
          return origBeforeSerialize.apply(this, arguments);
        }
      };

      // Re-enable on success.
      proto.success = function (response, status) {
        reenableInputs(this.svEfIsoDisabled);
        this.svEfIsoDisabled = null;

        if (typeof origSuccess === 'function') {
          return origSuccess.apply(this, arguments);
        }
      };

      // Re-enable on error.
      proto.error = function (xhr, uri, customMessage) {
        reenableInputs(this.svEfIsoDisabled);
        this.svEfIsoDisabled = null;

        if (typeof origError === 'function') {
          return origError.apply(this, arguments);
        }
      };
    }
  };
})(jQuery, Backdrop);

/* Note:
This patch is for the case where a subview contains an editablefield, and the parent view also 
has an editablefield. The problem is that when the parent view’s editablefield is submitted, 
the nested subview’s editablefield form is also serialized, which can cause unexpected behavior 
(e.g., if the subview’s form has required fields that are not filled out).

This is a containment fix. The structural “pure” fix is “don’t embed a Views form inside another 
Views form” (iframe or external host), but the above patch typically makes editablefields behave 
correctly in the parent while still allowing the child to function independently.

In PHP, duplicate scalar keys don’t merge—the later form_build_id/form_token/form_id wins. 
So the request is effectively processed as the child form while the triggering element is 
submit-field_billable-0 (parent). That mismatch is why it “does nothing” and why you see random 
field_bill_me[...] values leaking into the request.

This happens because the subview contains its own Views form and you’re embedding it inside 
the parent view’s form (nested forms are invalid HTML, but browsers allow it and .serialize() 
collects everything).
*/
