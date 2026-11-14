package com.tharunbirla.librecuts.shaders

import android.content.Context
import android.os.Build
import android.graphics.Bitmap
import java.io.Closeable
import java.io.File

/** Owns a real Vulkan instance, physical device, logical device and compute queue. */
class VulkanShaderRuntime private constructor(private var nativeHandle: Long) : Closeable {
    data class DeviceInfo(
        val name: String,
        val apiVersion: Int,
        val deviceType: Int,
        val supportsCompute: Boolean = true
    )

    val deviceInfo: DeviceInfo
        get() {
            check(nativeHandle != 0L) { "Vulkan runtime is closed" }
            return DeviceInfo(
                name = nativeDeviceName(nativeHandle),
                apiVersion = nativeApiVersion(nativeHandle),
                deviceType = nativeDeviceType(nativeHandle)
            )
        }

    /**
     * Runs a compatible SPIR-V compute effect on an ARGB_8888 bitmap.
     * Shader contract: set 0/binding 0 = readonly uint pixel buffer, binding 1 = writeonly
     * uint pixel buffer, push constants = uint width, uint height, local size = 8x8x1.
     * Each invocation must bounds-check gl_GlobalInvocationID against width/height.
     */
    fun renderBitmap(input: Bitmap, spirvFile: File): Bitmap {
        check(nativeHandle != 0L) { "Vulkan runtime is closed" }
        require(input.width > 0 && input.height > 0)
        require(spirvFile.isFile && spirvFile.extension.equals("spv", ignoreCase = true)) {
            "A compiled .spv compute shader is required"
        }
        val shader = spirvFile.readBytes()
        require(shader.size in 20..(16 * 1024 * 1024) && shader.size % 4 == 0) {
            "Invalid or oversized SPIR-V module"
        }
        val source = if (input.config == Bitmap.Config.ARGB_8888) input else input.copy(Bitmap.Config.ARGB_8888, false)
            ?: error("Could not convert input bitmap to ARGB_8888")
        val pixels = IntArray(source.width * source.height)
        source.getPixels(pixels, 0, source.width, 0, 0, source.width, source.height)
        val rendered = nativeRenderRgba(nativeHandle, shader, pixels, source.width, source.height)
            ?: error("Vulkan compute shader execution failed or the module uses an incompatible interface")
        val output = Bitmap.createBitmap(source.width, source.height, Bitmap.Config.ARGB_8888)
        output.setPixels(rendered, 0, source.width, 0, 0, source.width, source.height)
        if (source !== input) source.recycle()
        return output
    }

    override fun close() {
        val handle = nativeHandle
        if (handle != 0L) {
            nativeHandle = 0L
            nativeDestroy(handle)
        }
    }

    companion object {
        private val libraryLoaded: Boolean = try {
            System.loadLibrary("librecuts_vulkan")
            true
        } catch (_: UnsatisfiedLinkError) {
            false
        }

        /** Returns null on unsupported devices or when the native Vulkan runtime cannot initialize. */
        fun create(context: Context): VulkanShaderRuntime? {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N || !libraryLoaded) return null
            val feature = context.packageManager.hasSystemFeature("android.hardware.vulkan.version") ||
                context.packageManager.hasSystemFeature("android.hardware.vulkan.level")
            if (!feature) return null
            val handle = nativeCreate()
            return if (handle != 0L) VulkanShaderRuntime(handle) else null
        }

        @JvmStatic private external fun nativeCreate(): Long
        @JvmStatic private external fun nativeDestroy(handle: Long)
        @JvmStatic private external fun nativeDeviceName(handle: Long): String
        @JvmStatic private external fun nativeApiVersion(handle: Long): Int
        @JvmStatic private external fun nativeDeviceType(handle: Long): Int
        @JvmStatic private external fun nativeRenderRgba(
            handle: Long, shader: ByteArray, pixels: IntArray, width: Int, height: Int
        ): IntArray?
    }
}
