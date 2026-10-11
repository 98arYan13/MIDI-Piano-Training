export async function songApi<T>(
  path: string,
  options: RequestInit = {},
): Promise<T> {
  const response = await fetch(`/api/v1/songs${path}`, options);
  const body = await response.json();
  if (!response.ok)
    throw new Error(body.error?.message ?? "Library request failed");
  return body as T;
}
