/*
 * Copyright (c) 2021, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/String.h>
#include <AK/Types.h>
#include <Kernel/Graphics/Console/FramebufferConsole.h>
#include <Kernel/Graphics/GraphicsDevice.h>
#include <Kernel/PCI/DeviceController.h>
#include <Kernel/PhysicalAddress.h>

namespace Kernel {

class BochsFramebufferDevice;
class GraphicsManagement;
class BochsGraphicsAdapter final : public GraphicsDevice
    , public PCI::DeviceController {
    AK_MAKE_ETERNAL
    friend class BochsFramebufferDevice;
    friend class GraphicsManagement;

public:
    static NonnullRefPtr<BochsGraphicsAdapter> initialize(PCI::Address);
    virtual ~BochsGraphicsAdapter() = default;
    virtual bool framebuffer_devices_initialized() const override { return !m_framebuffer_device.is_null(); }

private:
    // ^GraphicsDevice
    virtual void initialize_framebuffer_devices() override;
    virtual Type type() const override;

    virtual void enable_consoles() override;
    virtual void disable_consoles() override;

    explicit BochsGraphicsAdapter(PCI::Address);

    void set_safe_resolution();

    bool validate_setup_resolution(size_t width, size_t height);
    u32 find_framebuffer_address();
    bool try_to_set_resolution(size_t width, size_t height);
    bool set_resolution(size_t width, size_t height);
    void set_resolution_registers(size_t width, size_t height);
    void set_y_offset(size_t);

    PhysicalAddress m_mmio_registers;
    RefPtr<BochsFramebufferDevice> m_framebuffer_device;
    RefPtr<Graphics::FramebufferConsole> m_framebuffer_console;
    SpinLock<u8> m_console_mode_switch_lock;
    bool m_console_enabled { false };
};

}
