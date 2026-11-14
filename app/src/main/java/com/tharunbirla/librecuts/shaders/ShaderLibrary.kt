package com.tharunbirla.librecuts.shaders

import android.content.Context
import android.net.Uri
import java.io.File
import java.util.zip.ZipInputStream

/**
 * Private, app-local storage for user-imported shader packs.
 * Imports .slang, .slangp, .glsl and ZIP shader bundles without executing them.
 */
class ShaderLibrary(context: Context) {
    private val root = File(context.filesDir, "shader-library").apply { mkdirs() }

    data class ImportResult(val directory: File, val files: List<File>, val presets: List<File>)

    fun importUri(context: Context, uri: Uri, displayName: String): ImportResult {
        val safeName = displayName.substringAfterLast('/').substringAfterLast('\\')
        require(safeName.isNotBlank()) { "A filename is required" }
        val extension = safeName.substringAfterLast('.', "").lowercase()
        require(extension in setOf("slang", "slangp", "glsl", "glslp", "spv", "zip")) {
            "Supported imports: .slang, .slangp, .glsl, .glslp, .spv or a ZIP shader bundle"
        }

        val packDir = File(root, "${System.currentTimeMillis()}-${safeName.substringBeforeLast('.', safeName).replace(Regex("[^A-Za-z0-9._-]"), "_")}")
        require(packDir.mkdirs()) { "Could not create shader pack directory" }
        try {
            val imported = if (extension == "zip") {
                val input = context.contentResolver.openInputStream(uri)
                    ?: error("Unable to open selected ZIP")
                input.use { zip -> extractZip(zip, packDir) }
            } else {
                val destination = File(packDir, safeName)
                context.contentResolver.openInputStream(uri)?.use { source ->
                    destination.outputStream().use { output -> source.copyTo(output) }
                } ?: error("Unable to open selected shader")
                listOf(destination)
            }
            val presets = imported.filter { it.extension.equals("slangp", true) }
            return ImportResult(packDir, imported, presets)
        } catch (error: Exception) {
            packDir.deleteRecursively()
            throw error
        }
    }

    fun listImportedFiles(): List<File> = root.walkTopDown()
        .filter { it.isFile && it.extension.lowercase() in setOf("slang", "slangp", "glsl", "glslp", "spv") }
        .sortedBy { it.name.lowercase() }
        .toList()

    private fun extractZip(input: java.io.InputStream, destination: File): List<File> {
        val extracted = mutableListOf<File>()
        var totalBytes = 0L
        ZipInputStream(input).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (entry.isDirectory) continue
                val relative = entry.name.replace('\\', '/')
                require(RetroArchSlangPresetParser.validateRelativeAssetPath(relative)) {
                    "Unsafe path in shader archive"
                }
                val target = File(destination, relative).canonicalFile
                require(target.path.startsWith(destination.canonicalPath + File.separator)) {
                    "Shader archive entry escaped its destination"
                }
                val extension = target.extension.lowercase()
                if (extension !in setOf("slang", "slangp", "glsl", "glslp", "spv", "png", "jpg", "jpeg", "bmp", "cube")) {
                    zip.closeEntry()
                    continue
                }
                target.parentFile?.mkdirs()
                target.outputStream().use { output ->
                    val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                    while (true) {
                        val count = zip.read(buffer)
                        if (count <= 0) break
                        totalBytes += count
                        require(totalBytes <= MAX_UNPACKED_BYTES) { "Shader archive exceeds 128 MB" }
                        output.write(buffer, 0, count)
                    }
                }
                extracted += target
                zip.closeEntry()
            }
        }
        require(extracted.any { it.extension.lowercase() in setOf("slang", "slangp", "glsl", "glslp", "spv") }) {
            "The ZIP does not contain a supported shader"
        }
        return extracted
    }

    companion object {
        private const val MAX_UNPACKED_BYTES = 128L * 1024L * 1024L
    }
}
