import { useEffect, useRef, useState } from "react";
export function NumericInput({
  value,
  min,
  max,
  integer = false,
  ariaLabel,
  onCommit,
}: {
  value: number;
  min: number;
  max: number;
  integer?: boolean;
  ariaLabel?: string;
  onCommit: (value: number) => void;
}) {
  const focused = useRef(false);
  const formatted = String(Math.round(value * 100) / 100);
  const [draft, setDraft] = useState(formatted);
  const [error, setError] = useState<string | null>(null);
  useEffect(() => {
    if (!focused.current) setDraft(formatted);
  }, [value]);
  const commit = () => {
    focused.current = false;
    const n = Number(draft);
    if (
      draft.trim() &&
      Number.isFinite(n) &&
      n >= min &&
      n <= max &&
      (!integer || Number.isInteger(n))
    ) {
      setError(null);
      onCommit(n);
    } else {
      setDraft(formatted);
      setError(
        `Enter ${integer ? "a whole number" : "a number"} from ${min} to ${max}.`,
      );
    }
  };
  return (
    <>
      <input
        type="number"
        aria-label={ariaLabel}
        min={min}
        max={max}
        step={integer ? 1 : "any"}
        value={draft}
        aria-invalid={!!error}
        onFocus={() => {
          focused.current = true;
          setError(null);
        }}
        onChange={(e) => setDraft(e.target.value)}
        onBlur={commit}
        onKeyDown={(e) => {
          if (e.key === "Enter") {
            e.preventDefault();
            e.currentTarget.blur();
          }
        }}
      />
      {error && (
        <small className="field-error" role="alert">
          {error}
        </small>
      )}
    </>
  );
}
