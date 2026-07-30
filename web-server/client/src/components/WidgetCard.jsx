import { useState, useRef, useEffect } from 'react';
import { ChevronDown, ChevronUp } from 'lucide-react';
import { useT } from '../i18n';

export default function WidgetCard({
  children,
  className = '',
  targetHeight = 230,
}) {
  const t = useT();
  const [isExpanded, setIsExpanded] = useState(false);
  const [hasOverflow, setHasOverflow] = useState(false);
  const contentRef = useRef(null);

  useEffect(() => {
    const checkOverflow = () => {
      if (contentRef.current) {
        // Measure if the inner content exceeds target height (plus small margin tolerance)
        const isOverflowing = contentRef.current.scrollHeight > targetHeight + 4;
        setHasOverflow(isOverflowing);
      }
    };

    checkOverflow();

    const observer = new ResizeObserver(() => {
      checkOverflow();
    });

    if (contentRef.current) {
      observer.observe(contentRef.current);
    }

    return () => observer.disconnect();
  }, [children, targetHeight]);

  return (
    <div className={`panel relative flex flex-col justify-between overflow-hidden transition-all duration-200 ${className}`}>
      <div
        ref={contentRef}
        className={`relative transition-[max-height] duration-300 ease-in-out ${
          isExpanded ? 'max-h-[2000px]' : 'overflow-hidden'
        }`}
        style={{
          minHeight: `${targetHeight}px`,
          height: isExpanded ? 'auto' : `${targetHeight}px`,
        }}
      >
        {children}

        {/* Gradient Overlay when collapsed and content overflows */}
        {hasOverflow && !isExpanded && (
          <div className="absolute inset-x-0 bottom-0 h-16 bg-gradient-to-t from-surface-container via-surface-container/90 to-transparent pointer-events-none" />
        )}
      </div>

      {/* Expand / Collapse toggle button */}
      {hasOverflow && (
        <div className="py-1.5 px-4 border-t border-outline-variant/30 flex justify-center bg-surface-container shrink-0 z-10">
          <button
            type="button"
            onClick={() => setIsExpanded((prev) => !prev)}
            className="inline-flex items-center gap-1.5 font-label-caps text-xs text-primary hover:text-primary-fixed transition-colors py-0.5 px-3 rounded hover:bg-surface-container-high active:scale-95"
          >
            {isExpanded ? (
              <>
                <span>{t('common.showLess')}</span>
                <ChevronUp size={14} />
              </>
            ) : (
              <>
                <span>{t('common.showAll')}</span>
                <ChevronDown size={14} />
              </>
            )}
          </button>
        </div>
      )}
    </div>
  );
}
