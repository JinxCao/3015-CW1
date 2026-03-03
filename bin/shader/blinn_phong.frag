#version 460

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in mat3 TBN;

out vec4 FragColor;

uniform vec3 viewPos;

uniform sampler2D baseColorMap;
uniform sampler2D normalMap;
uniform bool hasBaseColor;
uniform bool hasNormalMap;

struct DirLight {
    bool on;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    bool on;
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform DirLight dirLight;
uniform SpotLight spotLight;

float shininess = 32.0;

vec3 getNormal()
{
    if (hasNormalMap) {
        vec3 n = texture(normalMap, TexCoord).rgb * 2.0 - 1.0;
        return normalize(TBN * n);
    }
    return normalize(Normal);
}

vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 albedo)
{
    if (!light.on) return vec3(0.0);
    
    vec3 lightDir = normalize(-light.direction);
    vec3 halfDir = normalize(lightDir + viewDir);
    
    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
    
    vec3 ambient = light.ambient * albedo;
    vec3 diffuse = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec;
    
    return ambient + diffuse + specular;
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo)
{
    if (!light.on) return vec3(0.0);
    
    vec3 lightDir = normalize(light.position - fragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    
    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
    
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    vec3 ambient = light.ambient * albedo;
    vec3 diffuse = light.diffuse * diff * albedo;
    vec3 specular = light.specular * spec;
    
    return (ambient + diffuse + specular) * attenuation * intensity;
}

void main()
{
    vec3 albedo = hasBaseColor ? texture(baseColorMap, TexCoord).rgb : vec3(0.6, 0.6, 0.7);
    vec3 norm = getNormal();
    vec3 viewDir = normalize(viewPos - FragPos);

    if (!gl_FrontFacing) {
        norm = -norm;
    }
    
    vec3 result = calcDirLight(dirLight, norm, viewDir, albedo);
    result += calcSpotLight(spotLight, norm, FragPos, viewDir, albedo);
    
    FragColor = vec4(result, 1.0);
}
