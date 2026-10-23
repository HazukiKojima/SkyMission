using System.Text;

if (args.Length != 3)
{
    Console.Error.WriteLine("Usage: AircraftParameterGenerator <input-header> <output-csharp> <output-cpp-header>");
    return 2;
}

try
{
    string source = File.ReadAllText(args[0]);
    IReadOnlyList<string> fields = CppStructParser.ParseFloatFields(source, "AircraftFlightParameters");
    WriteGeneratedFile(args[1], CSharpGenerator.Generate(fields));
    WriteGeneratedFile(args[2], CppGenerator.GenerateLayoutAssertions(fields));
    return 0;
}
catch (Exception exception)
{
    Console.Error.WriteLine(exception.Message);
    return 1;
}

static void WriteGeneratedFile(string path, string content)
{
    string outputPath = Path.GetFullPath(path);
    Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);
    File.WriteAllText(outputPath, content, new UTF8Encoding(false));
}

static class CppGenerator
{
    public static string GenerateLayoutAssertions(IReadOnlyList<string> fields)
    {
        var source = new StringBuilder();
        source.AppendLine("#pragma once");
        source.AppendLine("#include \"../../../Engine/Core/AircraftFlightParameters.h\"");
        source.AppendLine("#include <cstddef>");
        source.AppendLine("#include <type_traits>");
        source.AppendLine();
        source.AppendLine("static_assert(std::is_standard_layout_v<Engine::AircraftFlightParameters>);");
        for (int index = 0; index < fields.Count; index++)
        {
            source.AppendLine($"static_assert(offsetof(Engine::AircraftFlightParameters, {fields[index]}) == {index} * sizeof(float));");
        }
        source.AppendLine($"static_assert(sizeof(Engine::AircraftFlightParameters) == {fields.Count} * sizeof(float));");
        return source.ToString();
    }
}

static class CSharpGenerator
{
    public static string Generate(IReadOnlyList<string> fields)
    {
        var source = new StringBuilder();
        source.AppendLine("namespace REDoxWrapper.Generated;");
        source.AppendLine();
        source.AppendLine("public sealed class GeneratedAircraftFlightParameters");
        source.AppendLine("{");
        source.AppendLine($"    public const int FieldCount = {fields.Count};");
        source.AppendLine("    public static readonly string[] FieldNames =");
        source.AppendLine("    {");
        foreach (string field in fields)
        {
            source.AppendLine($"        \"{field}\",");
        }
        source.AppendLine("    };");
        source.AppendLine();
        foreach (string field in fields)
        {
            source.AppendLine($"    public float {field} {{ get; set; }}");
        }
        source.AppendLine();
        source.AppendLine("    public float[] ToFloatArray() => new float[]");
        source.AppendLine("    {");
        foreach (string field in fields)
        {
            source.AppendLine($"        {field},");
        }
        source.AppendLine("    };");
        source.AppendLine("}");
        return source.ToString();
    }
}

static class CppStructParser
{
    public static IReadOnlyList<string> ParseFloatFields(string source, string structName)
    {
        List<string> tokens = Tokenize(source);
        for (int index = 0; index < tokens.Count - 2; index++)
        {
            if (tokens[index] != "struct" || tokens[index + 1] != structName)
            {
                continue;
            }

            index += 2;
            if (tokens[index] != "{")
            {
                throw new FormatException($"Expected '{{' after struct {structName}.");
            }

            var fields = new List<string>();
            var names = new HashSet<string>(StringComparer.Ordinal);
            index++;
            while (index < tokens.Count && tokens[index] != "}")
            {
                string type = tokens[index++];
                if (index >= tokens.Count)
                {
                    throw new FormatException($"Unexpected end of struct {structName}.");
                }
                string field = tokens[index++];
                if (type != "float")
                {
                    throw new FormatException($"Unsupported C++ type '{type}' for field '{field}'. Only float is supported.");
                }
                if (!IsIdentifier(field) || !names.Add(field))
                {
                    throw new FormatException($"Invalid or duplicate field name '{field}'.");
                }
                if (index < tokens.Count && tokens[index] == "=")
                {
                    index++;
                    if (index >= tokens.Count || !IsFloatLiteral(tokens[index]))
                    {
                        throw new FormatException($"Field '{field}' must use a numeric float initializer.");
                    }
                    index++;
                }
                if (index >= tokens.Count || tokens[index] != ";")
                {
                    throw new FormatException($"Expected ';' after field '{field}'.");
                }
                index++;
                fields.Add(field);
            }

            if (index >= tokens.Count || tokens[index] != "}" || fields.Count == 0)
            {
                throw new FormatException($"Struct {structName} is empty or unterminated.");
            }
            return fields;
        }

        throw new FormatException($"Struct {structName} was not found.");
    }

    private static List<string> Tokenize(string source)
    {
        var tokens = new List<string>();
        for (int index = 0; index < source.Length;)
        {
            char current = source[index];
            if (char.IsWhiteSpace(current))
            {
                index++;
                continue;
            }
            if (current == '/' && index + 1 < source.Length && source[index + 1] == '/')
            {
                index += 2;
                while (index < source.Length && source[index] != '\n') index++;
                continue;
            }
            if (current == '/' && index + 1 < source.Length && source[index + 1] == '*')
            {
                int end = source.IndexOf("*/", index + 2, StringComparison.Ordinal);
                if (end < 0) throw new FormatException("Unterminated C++ block comment.");
                index = end + 2;
                continue;
            }
            if (char.IsLetter(current) || current == '_')
            {
                int start = index++;
                while (index < source.Length && (char.IsLetterOrDigit(source[index]) || source[index] == '_')) index++;
                tokens.Add(source[start..index]);
                continue;
            }
            if (char.IsDigit(current) || current == '-' && index + 1 < source.Length && char.IsDigit(source[index + 1]))
            {
                int start = index++;
                while (index < source.Length && (char.IsLetterOrDigit(source[index]) || source[index] is '.' or '+' or '-')) index++;
                tokens.Add(source[start..index]);
                continue;
            }
            tokens.Add(current.ToString());
            index++;
        }
        return tokens;
    }

    private static bool IsIdentifier(string value) =>
        value.Length > 0 && (char.IsLetter(value[0]) || value[0] == '_') &&
        value.Skip(1).All(character => char.IsLetterOrDigit(character) || character == '_');

    private static bool IsFloatLiteral(string value) =>
        float.TryParse(value.TrimEnd('f', 'F'), System.Globalization.NumberStyles.Float,
            System.Globalization.CultureInfo.InvariantCulture, out _);
}
