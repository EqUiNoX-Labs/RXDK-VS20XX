using System.Text.Json;
using System.Text.Json.Serialization;

namespace Rxdk.Engine.Model;

/// <summary>
/// Loads and parses rxdk.project.json. Mirrors the reads in RXDK-VSCode
/// (stripBom + JSON.parse). Manifests are camelCase with string enums
/// ("executable"/"library"/"dxt", "debug"/"release").
/// </summary>
public static class RxdkManifestLoader
{
    public const string ManifestFileName = "rxdk.project.json";

    public static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase) },
    };

    /// <summary>Strip a UTF-8 BOM if present (port of xboxSdkPaths.ts stripBom).</summary>
    public static string StripBom(string text) =>
        text.Length > 0 && text[0] == '﻿' ? text[1..] : text;

    /// <summary>Parse a manifest from raw JSON text. Throws on malformed JSON.</summary>
    public static RxdkProjectManifest Parse(string json)
    {
        var manifest = JsonSerializer.Deserialize<RxdkProjectManifest>(StripBom(json), JsonOptions)
            ?? throw new InvalidDataException($"{ManifestFileName} parsed to null");
        return manifest;
    }

    /// <summary>Load the manifest at &lt;projectRoot&gt;/rxdk.project.json.</summary>
    public static RxdkProjectManifest Load(string projectRoot)
    {
        var path = Path.Combine(projectRoot, ManifestFileName);
        return Parse(File.ReadAllText(path));
    }

    /// <summary>Try to load a manifest; returns null instead of throwing on missing/invalid.</summary>
    public static RxdkProjectManifest? TryLoad(string projectRoot)
    {
        try
        {
            var path = Path.Combine(projectRoot, ManifestFileName);
            return File.Exists(path) ? Parse(File.ReadAllText(path)) : null;
        }
        catch
        {
            return null;
        }
    }
}
