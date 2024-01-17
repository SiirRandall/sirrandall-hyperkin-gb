/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hid_device_driver.h"

extern pad_connection_interface_t null_pad_connection;

/*
 * This is the instance data structure for the pad you are implementing.
 * This is a good starting point, but you can add/remove things as makes
 * sense for the pad you're writing for. The pointer to this structure
 * will be passed in as a void pointer to the methods you implement below.
 */
typedef struct null_instance {
   void *handle;             /* a handle to the HID subsystem adapter */
   joypad_connection_t *pad; /* a pointer to the joypad connection you assign
                                in init() */
   int slot;                 /* which slot does this pad occupy? */
   uint32_t buttons;         /* a bitmap of the digital buttons for the pad */
   uint16_t motors[2];       /* rumble strength, if appropriate */
   uint8_t data[64];         /* a buffer large enough to hold the device's
                                max rx packet */
} null_instance_t;

/**
 * Use the HID_ macros (see input/include/hid_driver.h) to send data packets
 * to the device. When this method returns, the device needs to be in a state
 * where we can read data packets from the device. So, if there's any
 * activation packets (see the ds3 and Wii U GameCube adapter drivers for
 * examples), send them here.
 *
 * While you *can* allocate the retro pad here, it isn't mandatory (see
 * the Wii U GC adapter).
 *
 * If initialization fails, return NULL.
 */
static void *null_init(void *handle)
{
   null_instance_t *instance;
   instance = (null_instance_t *)calloc(1, sizeof(null_instance_t));
   if(!instance)
      goto error;

   memset(instance, 0, sizeof(null_instance_t));
   instance->handle = handle;
   instance->pad = hid_pad_register(instance, &null_pad_connection);
   if(!instance->pad)
      goto error;

   RARCH_LOG("[null]: init complete.\n");
   return instance;

   error:
      RARCH_ERR("[null]: init failed.\n");
      if(instance)
         free(instance);

      return NULL;
}

/*
 * Gets called when the pad is disconnected. It must clean up any memory
 * allocated and used by the instance data.
 */
static void null_free(void *data)
{
   null_instance_t *instance = (null_instance_t *)data;

   if(instance) {
      hid_pad_deregister(instance->pad);
      free(instance);
   }
}

/**
 * Handle a single packet from the device.
 * For most pads you'd just forward it onto the pad driver (see below).
 * A more complicated example is in the Wii U GC adapter driver.
 */
static void null_handle_packet(void *data, uint8_t *buffer, size_t size)
{
   null_instance_t *instance = (null_instance_t *)data;

   if(instance && instance->pad)
      instance->pad->iface->packet_handler(instance->pad->data, buffer, size);
}

/**
 * Return true if the passed in VID and PID are supported by the driver.
 */
static bool null_detect(uint16_t vendor_id, uint16_t product_id)
{
  return vendor_id == VID_NONE && product_id == PID_NONE;
}

/**
 * Assign function pointers to the driver structure.
 */
hid_device_t null_hid_device = {
  null_init,
  null_free,
  null_handle_packet,
  null_detect,
  "Null HID device"
};

/**
 * This is called via hid_pad_register(). In the common case where the
 * device only controls one pad, you can simply return the data parameter.
 * But if you need to track multiple pads attached to the same HID device
 * (see: Wii U GC adapter), you can allocate that memory here.
 */
static void *null_pad_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   null_instance_t *instance = (null_instance_t *)data;

   if(!instance)
      return NULL;

   instance->slot = slot;
   return instance;
}

/**
 * If you allocate any memory in null_pad_init() above, de-allocate it here.
 */
static void null_pad_deinit(void *data)
{
}

/**
 * Translate the button data from the pad into the input_bits_t format
 * that RetroArch can use.
 */
static void null_get_buttons(void *data, input_bits_t *state)
{
   null_instance_t *instance = (null_instance_t *)data;
   if(!instance)
      return;

   /* TODO: get buttons */
}

/**
 * Handle a single packet for the pad.
 */
static void null_packet_handler(void *data, uint8_t *packet, uint16_t size)
{
   null_instance_t *instance = (null_instance_t *)data;
   if(!instance)
      return;

   RARCH_LOG_BUFFER(packet, size);
}

/**
 * If the pad doesn't support rumble, then this can just be a no-op.
 */
static void null_set_rumble(void *data, enum retro_rumble_effect effect, uint16_t strength)
{
}

/**
 * Read analog sticks.
 * If the pad doesn't have any analog axis, just return 0 here.
 *
 * The return value must conform to the following characteristics:
 * - (0, 0) is center
 * - (-32768,-32768) is top-left
 * - (32767,32767) is bottom-right
 */
static int16_t null_get_axis(void *data, unsigned axis)
{
   return 0;
}

/**
 * The name the pad will show up as in the UI, also used to auto-assign
 * buttons in input/input_autodetect_builtin.c
 */
static const char *null_get_name(void *data)
{
   return "Null HID Pad";
}

/**
 * Read the state of a single button.
 */
static bool null_button(void *data, uint16_t joykey)
{
  return false;
}

/**
 * Fill in the joypad interface
 */
pad_connection_interface_t null_pad_connection = {
   null_pad_init,
   null_pad_deinit,
   null_packet_handler,
   null_set_rumble,
   null_get_buttons,
   null_get_axis,
   null_get_name,
   null_button
};
