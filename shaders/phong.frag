#version 330 core

struct Material{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct PointLight {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

in vec3 fragPos;

out vec4 FragColor;

uniform vec3 view_pos;
uniform vec3 light_pos;

const Material material = Material(
    vec3(0.2, 0.5, 1.0),
    vec3(0.2, 0.5, 1.0),
    vec3(0.5),
    32.f
);
const PointLight light = PointLight(
    vec3(0.1),
    vec3(1.0),
    vec3(1.0),
    1.0,
    0.02,
    1.7e-3
);

void main(){
    vec3 normal = fragPos;

    // ambient
    vec3 ambient = light.ambient * material.ambient;

    // diffuse
    vec3 lightDir = normalize(light_pos - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);

    // specular
    vec3 viewDir = normalize(view_pos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);

    // attenuation
    float d = distance(light_pos, fragPos);
    float attenuation = 1.0 / (light.constant + light.linear*d + light.quadratic*d*d);

    vec3 result = attenuation * (ambient + diffuse + specular);
    FragColor = vec4(result, 1.0);
}