package com.tharunbirla.librecuts.shaders

/** Parsed, non-executable description of a RetroArch Slang preset. */
data class RetroArchShaderParameter(
    val name: String,
    val description: String,
    val defaultValue: String,
    val minimum: String?,
    val maximum: String?,
    val step: String?
)

data class RetroArchSlangPreset(
    val title: String,
    val shaderPaths: List<String>,
    val textures: List<String>,
    val parameters: List<RetroArchShaderParameter>,
    val source: String
) {
    val passCount: Int get() = shaderPaths.size
}

/**
 * Reads the INI-like .slangp format and parameter pragmas from .slang files.
 * This deliberately does not claim to compile or execute Slang shaders.
 */
object RetroArchSlangPresetParser {
    private val shaderKey = Regex("^shader(\\d+)$", RegexOption.IGNORE_CASE)
    private val parameterPragma = Regex(
        """^\s*#pragma\s+parameter\s+(\S+)\s+"([^"]*)"\s+([^\s]+)(?:\s+([^\s]+))?(?:\s+([^\s]+))?(?:\s+([^\s]+))?"""
    )

    fun parsePreset(text: String, fallbackTitle: String = "Imported Slang preset"): RetroArchSlangPreset {
        val values = linkedMapOf<String, String>()
        text.lineSequence().forEach { rawLine ->
            val line = rawLine.substringBefore('#').trim()
            if (line.isEmpty() || line.startsWith('[') || !line.contains('=')) return@forEach
            val key = line.substringBefore('=').trim()
            val value = line.substringAfter('=').trim().trim('"', '\'')
            if (key.isNotEmpty()) values[key] = value
        }

        val shaders = values.entries.mapNotNull { (key, value) ->
            val match = shaderKey.matchEntire(key) ?: return@mapNotNull null
            match.groupValues[1].toIntOrNull()?.let { it to value }
        }.sortedBy { it.first }.map { it.second }

        val textures = values.entries
            .filter { it.key.startsWith("textures", ignoreCase = true) }
            .flatMap { (_, value) -> value.split(';', ',').map(String::trim).filter(String::isNotEmpty) }
            .distinct()

        return RetroArchSlangPreset(
            title = values["preset_name"]?.takeIf(String::isNotBlank) ?: fallbackTitle,
            shaderPaths = shaders,
            textures = textures,
            parameters = emptyList(),
            source = text
        )
    }

    fun parseParameters(shaderSource: String): List<RetroArchShaderParameter> =
        shaderSource.lineSequence().mapNotNull { line ->
            val match = parameterPragma.find(line) ?: return@mapNotNull null
            RetroArchShaderParameter(
                name = match.groupValues[1],
                description = match.groupValues[2],
                defaultValue = match.groupValues[3],
                minimum = match.groupValues[4].ifBlank { null },
                maximum = match.groupValues[5].ifBlank { null },
                step = match.groupValues[6].ifBlank { null }
            )
        }.toList()

    fun validateRelativeAssetPath(path: String): Boolean {
        if (path.isBlank() || path.startsWith('/') || path.startsWith('\\')) return false
        if (Regex("^[A-Za-z]:").containsMatchIn(path)) return false
        val normalized = path.replace('\\', '/')
        return normalized.split('/').none { it == ".." || it.isBlank() }
    }
}
