package com.tharunbirla.librecuts.shaders

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build

/** Hardware capability only; it does not imply that LibreCuts has a Vulkan renderer loaded. */
data class VulkanDeviceSupport(
    val hardwareFeaturePresent: Boolean,
    val hardwareLevel: Int?,
    val hardwareVersion: Int?,
    val apiLevel: Int
) {
    val isAvailable: Boolean get() = hardwareFeaturePresent

    companion object {
        fun inspect(context: Context): VulkanDeviceSupport {
            val pm = context.packageManager
            val supported = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                pm.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_LEVEL)
            } else false
            val level = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && supported) {
                pm.getSystemAvailableFeatures().firstOrNull {
                    it.name == PackageManager.FEATURE_VULKAN_HARDWARE_LEVEL
                }?.version
            } else null
            val version = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                pm.getSystemAvailableFeatures().firstOrNull {
                    it.name == PackageManager.FEATURE_VULKAN_HARDWARE_VERSION
                }?.version
            } else null
            return VulkanDeviceSupport(supported, level, version, Build.VERSION.SDK_INT)
        }
    }
}
