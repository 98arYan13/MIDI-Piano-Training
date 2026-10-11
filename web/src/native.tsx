import {
  createContext,
  useContext,
  useEffect,
  useState,
  type ReactNode,
} from "react";
import { NativeClient } from "./nativeClient";
const client = new NativeClient();
const Context = createContext(client);
export function NativeProvider({ children }: { children: ReactNode }) {
  useEffect(() => {
    client.connect();
    return () => client.dispose();
  }, []);
  return <Context.Provider value={client}>{children}</Context.Provider>;
}
export function useNative() {
  const value = useContext(Context);
  const [, render] = useState(0);
  useEffect(() => value.subscribe(() => render((n) => n + 1)), [value]);
  return value;
}
