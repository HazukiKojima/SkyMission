using System.Text;

if (args.Length == 0 || args.Length % 5 != 0)
{
    Console.Error.WriteLine("Usage: AircraftParameterGenerator <input-header> <struct-name> <class-name> <output-csharp> <output-cpp-header> [...]");
    return 2;
}

try
{
    using var generationMutex = new Mutex(false, "AircraftParameterGenerator");
    try
    {
        generationMutex.WaitOne();
    }
    catch (AbandonedMutexException)
    {
    }

    try
    {
        for (int argumentIndex = 0; argumentIndex < args.Length; argumentIndex += 5)
        {
            string headerPath = args[argumentIndex];
            string structName = args[argumentIndex + 1];
            string className = args[argumentIndex + 2];
            string source = File.ReadAllText(headerPath);
            IReadOnlyList<Field> fields = CppStructParser.ParseFloatFields(source, structName.Split("::")[^1]);
            WriteGeneratedFile(args[argumentIndex + 3], CSharpGenerator.Generate(fields, className));
            WriteGeneratedFile(args[argumentIndex + 4], CppGenerator.GenerateLayoutAssertions(
                fields, structName, Path.GetRelativePath(Path.GetDirectoryName(args[argumentIndex + 4])!, headerPath)));
        }
        return 0;
    }
    finally
    {
        generationMutex.ReleaseMutex();
    }
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
    if (File.Exists(outputPath) && File.ReadAllText(outputPath) == content)
    {
        return;
    }

    File.WriteAllText(outputPath, content, new UTF8Encoding(false));
}

static class CppGenerator
{
    public static string GenerateLayoutAssertions(IReadOnlyList<Field> fields, string structName, string headerPath)
    {
        var source = new StringBuilder();
        string qualifiedName = structName.Contains("::") ? structName : $"Engine::{structName}";
        source.AppendLine("#pragma once");
        source.AppendLine($"#include \"{headerPath.Replace('\\', '/')}\"");
        source.AppendLine("#include <cstddef>");
        source.AppendLine("#include <type_traits>");
        source.AppendLine();
        source.AppendLine($"static_assert(std::is_standard_layout_v<{qualifiedName}>);");
        for (int index = 0; index < fields.Count; index++)
        {
            source.AppendLine($"static_assert(offsetof({qualifiedName}, {fields[index].Name}) == {fields[index].Offset} * sizeof(float));");
        }
        source.AppendLine($"static_assert(sizeof({qualifiedName}) == {fields.Sum(field => field.Length)} * sizeof(float));");
        return source.ToString();
    }
}

static class CSharpGenerator
{
    public static string Generate(IReadOnlyList<Field> fields, string className)
    {
        var source = new StringBuilder();
        source.AppendLine("namespace REDoxWrapper.Generated;");
        source.AppendLine();
        source.AppendLine($"public sealed class {className}");
        source.AppendLine("{");
        source.AppendLine($"    public const int FieldCount = {fields.Sum(field => field.Length)};");
        source.AppendLine("    public static readonly string[] FieldNames =");
        source.AppendLine("    {");
        foreach (Field field in fields)
        {
            source.AppendLine($"        \"{field.Name}\",");
        }
        source.AppendLine("    };");
        source.AppendLine("    public static readonly int[] FieldLengths =");
        source.AppendLine("    {");
        foreach (Field field in fields)
        {
            source.AppendLine($"        {field.Length},");
        }
        source.AppendLine("    };");
        source.AppendLine();
        foreach (Field field in fields)
        {
            source.AppendLine(field.Length == 1
                ? $"    public float {field.Name} {{ get; set; }}"
                : $"    public float[] {field.Name} {{ get; set; }} = new float[{field.Length}];");
        }
        source.AppendLine();
        source.AppendLine("    public float[] ToFloatArray() => new float[]");
        source.AppendLine("    {");
        foreach (Field field in fields)
        {
            for (int index = 0; index < field.Length; index++)
            {
                source.AppendLine(field.Length == 1
                    ? $"        {field.Name},"
                    : $"        {field.Name}[{index}],");
            }
        }
        source.AppendLine("    };");
        source.AppendLine("}");
        return source.ToString();
    }
}

record Field(string Name, int Length, int Offset);

static class CppStructParser
{
    public static IReadOnlyList<Field> ParseFloatFields(string source, string structName)
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

            var fields = new List<Field>();
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
                int length = 1;
                if (index < tokens.Count && tokens[index] == "[")
                {
                    index++;
                    if (index >= tokens.Count || !int.TryParse(tokens[index++], out length) || length <= 0 ||
                        index >= tokens.Count || tokens[index++] != "]")
                    {
                        throw new FormatException($"Field '{field}' must use a positive fixed array length.");
                    }
                }
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
                fields.Add(new Field(field, length, fields.Sum(item => item.Length)));
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
