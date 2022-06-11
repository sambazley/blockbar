Module Types
------------
A module's type is set with ``data->type`` in the ``init`` function. Each type
is described below:

* ``BLOCK`` modules define a new type of block. Each block must be assigned to
  a ``BLOCK`` module.
* ``RENDER`` modules can draw to anywhere on the bar. Blocks are not assigned to
  ``RENDER`` modules.

Common Functions
----------------
This section provides a list of functions that are common to all module types.

``int init(struct module_data *data)``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This function is called when the module is loaded. It is used to initialize the
module. It has the following requirements:

* It must exist.
* ``data->name`` must be set. This should be set to the name of the module.
* It must return zero. Non-zero indicates an error occured.

If these requirements are not met, the module will fail to initialize.

``data`` also contains other optional variables:

* ``enum module_type type`` - The module's type. Defaults to ``BLOCK``.
* ``long flags`` - A flag field for setting module options. The flags each have
  a ``MFLAG_`` prefix and can be found in types.h.
* ``struct setting *settings`` - A pointer to the first setting in a list of
  settings.
* ``int setting_count`` - The number of settings in the list.
* ``int interval`` - The time interval between each call of a ``RENDER``
  module's ``render`` function. If this variable is set to zero, the ``render``
  function is called each time that the bar is redrawn. This variable is ignored
  if the module is not a ``RENDER`` module.

Below is a minimalistic example of an ``init`` function:

.. code-block:: c

    int init(struct module_data *data) {
        data->name = "example";
        return 0;
    }

``void unload()``
~~~~~~~~~~~~~~~~~

This function is called when the module is unloaded.

``void setting_update(struct setting *setting)``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function is called when a bar setting or module setting is changed.

``BLOCK`` Module Functions
--------------------------
This sections provides a list of functions that are specific to ``BLOCK``
modules.

``void block_add(struct block *blk)``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function is called when a block is registered to the module.

``void block_remove(struct block *blk)``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function is called when a block is unregistered from the module, or when a
block registered to the module is removed.

``int render(cairo_t *ctx, struct block *blk, int bar)``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function renders the contents of the block. Typically, this function
uses the ``execdata`` variable to determine what should be rendered, and
returns the total width of the rendered content. A return value of 0 indicates
that the block failed to render. Here is an example ``render`` function:

.. code-block:: c

    int render(cairo_t *ctx, struct block *blk, int bar) {
        int x = 0;
        char *execdata;

        if (blk->eachmon) {
            execdata = blk->data[bar].exec_data;
        } else {
            execdata = blk->data->exec_data;
        }

        if (!execdata) {
            return 0;
        }

        ...
        x += draw_xyz(...);
        ...

        return x;
    }

``int exec(struct block *blk, int bar, struct click *cd)``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function is called before a block is executed. If this was triggered by
the block being clicked, ``cd`` will be non zero and ``bar`` will be set to
the ID of the bar that was clicked. Otherwise, if the block has
``eachmon=true``, ``cd`` will be zero and ``bar`` will be set to a bar's ID. If
the block was not clicked and the block has ``eachmon=false``, both ``cd`` and
``bar`` will be zero.

``cd`` contains the following variables:

* ``int button`` - The mouse button that was pressed.
* ``int x`` - The X position of the cursor, relative to the left edge of the
  bar.
* ``int bar`` - The ID of the bar that the block is on. This is the same as the
  ``bar`` parameter of the function.

Additional environment variables can be set from this function using the
``blockbar_set_env`` function.

If the ``exec`` function returns non zero, the block's execution will be
cancelled.

``RENDER`` Module Functions
---------------------------

This section provides a list of functions that are specific to ``RENDER``
modules.

``int render(cairo_t *ctx, int bar)``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function renders to a cairo surface that is displayed on the bar. A return
value of 0 indicates that the block failed to render.

Module Flags
------------

* ``MFLAG_NO_EXEC`` - A script will not be executed for blocks assigned to a
  ``BLOCK`` module with this flag. The module's ``render`` function will be
  called with the block's ``execdata`` unset.
