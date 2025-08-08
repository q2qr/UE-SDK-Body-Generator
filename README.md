# UE-SDK-Body-Generator
A generator for Unreal Engine SDK dumps (made for Dumper-7) that fills stub methods that cause the  ```unresolved external symbol``` error
## How it works
The program takes advantage of the ```ProcessEvent``` method in the Unreal Engine internals to call methods without needing offsets for fixing stub methods

Before:
```cpp
void DestroyAndRemove() const;
	float GetAttenuationRadius() const;
	struct FVector GetColor() const;
```

After:
```cpp
// auto-generated function for DestroyAndRemove
void ALuauPointLight::DestroyAndRemove()
{
    ALuauPointLight_DestroyAndRemove_Params params;
    ProcessEvent(FindFunctionChecked(FName(TEXT("DestroyAndRemove"))), &params);
}

// auto-generated function for GetAttenuationRadius
float ALuauPointLight::GetAttenuationRadius()
{
    ALuauPointLight_GetAttenuationRadius_Params params;
    ProcessEvent(FindFunctionChecked(FName(TEXT("GetAttenuationRadius"))), &params);
    return params.ReturnValue;
}

// auto-generated function for GetColor
struct FVector ALuauPointLight::GetColor()
{
    ALuauPointLight_GetColor_Params params;
    ProcessEvent(FindFunctionChecked(FName(TEXT("GetColor"))), &params);
    return params.ReturnValue;
}
```
